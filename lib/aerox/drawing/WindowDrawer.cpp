#include "aerox/drawing/WindowDrawer.hpp"

#include "VkBootstrap.h"
#include "aerox/Engine.hpp"
#include "aerox/drawing/scene/SceneDrawer.hpp"
#include "aerox/widgets/WidgetRoot.hpp"
#include "aerox/window/Window.hpp"


namespace aerox::drawing {

void WindowDrawer::OnInit(const std::weak_ptr<window::Window> & window, DrawingSubsystem * drawer) {
  TObjectWithInit::OnInit(window,drawer);
  _window = window;
  _drawingSubsystem = drawer;

  const auto instance = _drawingSubsystem->GetVulkanInstance();
  _surface = _window.lock()->CreateSurface(instance);
  const auto newSize = _window.lock()->GetPixelSize();
  _viewport.x = 0;
  _viewport.y = 0;
  _viewport.width = newSize.x;
  _viewport.height = newSize.y;
  _viewport.minDepth = 0.0f;
  _viewport.maxDepth = 1.0f;
}

bool WindowDrawer::ShouldDraw() const {
  return IsInitialized() && _isReady && !_isResizing;
}

void WindowDrawer::CreateResources() {

  AddCleanup(_window.lock()->onResize->BindFunction(
                 [this](const std::weak_ptr<window::Window> &win) {
                   _isResizing = true;
                   const auto newSize = win.lock()->GetPixelSize();
                   if (newSize == glm::uvec2{0, 0}) {
                     return;
                   }
                   _viewport.width = newSize.x;
                   _viewport.height = newSize.y;
                   _drawingSubsystem->WaitDeviceIdle();
                   DestroySwapchain();
                   CreateSwapchain();
                   onResizeScenes->Execute();
                   onResizeUi->Execute();
                   _isResizing = false;
                   _isResizing = false;
                 }));

  CreateSwapchain();

  AddCleanup([this] {
    if (_swapchain != nullptr) {
      DestroySwapchain();
    }
  });

  InitFrames();
  InitSyncStructures();
  InitDescriptors();

  _isReady = true;
}

void WindowDrawer::CreateSwapchain() {
  const auto extent = GetSwapchainExtent();
  const auto device = GetVirtualDevice();
  const auto gpu = _drawingSubsystem->GetPhysicalDevice();
  vkb::SwapchainBuilder swapchainBuilder{gpu, device, _surface};

  _swapchainImageFormat = vk::Format::eB8G8R8A8Unorm;

  vkb::Swapchain vkbSwapchain = swapchainBuilder
                                .set_desired_format(
                                    vk::SurfaceFormatKHR(
                                        _swapchainImageFormat,
                                        vk::ColorSpaceKHR::eSrgbNonlinear))
                                // .set_desired_present_mode(
                                //     VK_PRESENT_MODE_FIFO_KHR)
  .set_desired_present_mode(static_cast<VkPresentModeKHR>(vk::PresentModeKHR::eFifo))
                                .set_desired_extent(extent.width, extent.height)
                                .add_image_usage_flags(
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                .build()
                                .value();

  _swapchain = vkbSwapchain.swapchain;
  const auto vkbSwapchainImages = vkbSwapchain.get_images().value();
  for (const auto image : vkbSwapchainImages) {
    vk::Image im = image;
    _swapchainImages.push(im);
  }

  const auto vkbSwapchainViews = vkbSwapchain.get_image_views().value();
  for (const auto image : vkbSwapchainViews) {
    vk::ImageView im = image;
    _swapchainImageViews.push(im);
  }
}

void WindowDrawer::DestroySwapchain() {
  const auto device = GetVirtualDevice();

  for (const auto view : _swapchainImageViews) {
    device.destroyImageView(view);
  }

  _swapchainImageViews.clear();
  _swapchainImages.clear();

  device.destroySwapchainKHR(_swapchain);

  _swapchain = nullptr;
}

vk::Device WindowDrawer::GetVirtualDevice() const {
  return _drawingSubsystem->GetVirtualDevice();
}

void WindowDrawer::InitFrames() {
  const auto device = GetVirtualDevice();

  const auto commandPoolInfo = vk::CommandPoolCreateInfo(
      vk::CommandPoolCreateFlags(
          vk::CommandPoolCreateFlagBits::eResetCommandBuffer),
      _drawingSubsystem->GetQueueFamily());

  for (auto &frame : _frames) {

    frame.SetCommandPool(device.createCommandPool(commandPoolInfo, nullptr));
    const auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo(
        *frame.GetCmdPool(), vk::CommandBufferLevel::ePrimary, 1);

    frame.SetCommandBuffer(
        device.allocateCommandBuffers(commandBufferAllocateInfo)
              .
              at(0));

    frame.SetDrawer(_drawingSubsystem);
    frame.SetWindowDrawer(this);
  }

  AddCleanup([this] {
    const auto device = GetVirtualDevice();

    for (auto &frame : _frames) {
      device.destroyCommandPool(*frame.GetCmdPool());
    }
  });
}

void WindowDrawer::InitSyncStructures() {
  const auto device = GetVirtualDevice();

  constexpr auto fenceCreateInfo = vk::FenceCreateInfo(
      vk::FenceCreateFlagBits::eSignaled);

  constexpr auto semaphoreCreateInfo = vk::SemaphoreCreateInfo(
      vk::SemaphoreCreateFlags());

  for (auto &frame : _frames) {
    frame.SetRenderFence(device.createFence(fenceCreateInfo));
    frame.SetSemaphores(device.createSemaphore(semaphoreCreateInfo),
                        device.createSemaphore(semaphoreCreateInfo));
  }

  AddCleanup([this] {
    const auto device = GetVirtualDevice();
    for (const auto &frame : _frames) {
      device.destroyFence(frame.GetRenderFence());
      device.destroySemaphore(frame.GetRenderSemaphore());
      device.destroySemaphore(frame.GetSwapchainSemaphore());
    }
  });
}

RawFrameData *WindowDrawer::GetCurrentFrame() {
  return &_frames[_frameCount % FRAME_OVERLAP];
}

void WindowDrawer::InitDescriptors() {

  const auto device = GetVirtualDevice();

  const auto allocator = _drawingSubsystem->GetGlobalDescriptorAllocator();

  for (auto &frame : _frames) {
    // create a descriptor pool
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
        {vk::DescriptorType::eStorageImage, 3},
        {vk::DescriptorType::eStorageBuffer, 3},
        {vk::DescriptorType::eUniformBuffer, 3},
        {vk::DescriptorType::eCombinedImageSampler, 4},
    };

    frame.SetDrawer(_drawingSubsystem);
    frame.GetDescriptorAllocator()->Init(device, 1000, frame_sizes);

    AddCleanup([&] {
      frame.GetDescriptorAllocator()->DestroyPools();
      frame.cleaner.Run();
    });
  }
}

vk::SurfaceKHR WindowDrawer::GetSurface() const {
  return _surface;
}

vk::Extent2D WindowDrawer::GetSwapchainExtent() const {
  const auto pixelSize = _window.lock()->GetPixelSize();
  return {static_cast<uint32_t>(pixelSize.x),
          static_cast<uint32_t>(pixelSize.y)};
}

vk::Extent2D WindowDrawer::GetSwapchainExtentScaled() const {

  const auto extent = GetSwapchainExtent();
  constexpr auto renderScale = 1.0f;
  return {static_cast<uint32_t>(renderScale * extent.width),
          static_cast<uint32_t>(renderScale * extent.height)};
}

vk::Format WindowDrawer::GetSwapchainFormat() const {
  return _swapchainImageFormat;
}

vk::Viewport WindowDrawer::GetViewport() const {
  return _viewport;
}

void WindowDrawer::Draw() {

  const auto frame = GetCurrentFrame();
  const auto device = GetVirtualDevice();
  // Wait for gpu to finish past work
  vk::resultCheck(
      device.waitForFences({frame->GetRenderFence()}, true, 1000000000),
      "Wait For Fences Failed");

  frame->cleaner.Run();
  frame->GetDescriptorAllocator()->ClearPools();

  device.resetFences({frame->GetRenderFence()});

  // Request image index from swapchain
  uint32_t swapchainImageIndex;

  try {
    const auto _ = device.acquireNextImageKHR(_swapchain, 1000000000,
                                              frame->GetSwapchainSemaphore(),
                                              nullptr, &swapchainImageIndex);
  } catch (vk::OutOfDateKHRError &_) {
    /* Resize here */
    _isResizing = true;
    return;
  }

  const auto cmd = frame->GetCmd();

  // Clear command buffer and prepare to render
  cmd->reset();

  constexpr auto commandBeginInfo = vk::CommandBufferBeginInfo(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  const auto swapchainExtent = GetSwapchainExtent();

  const vk::Extent2D drawExtent = GetSwapchainExtent();

  cmd->begin(commandBeginInfo);

  cmd->setViewport(0, {_viewport});

  vk::Rect2D scissor{
      {0, 0},
      {static_cast<uint32_t>(_viewport.width),
       static_cast<uint32_t>(_viewport.height)}};

  cmd->setScissor(0, {scissor});

  const auto willDrawScenes = onDrawScenes->GetNumListeners() > 0;
  const auto willDrawUi = onDrawUi->GetNumListeners() > 0;

  if (willDrawScenes) {
    onDrawScenes->Execute(frame);
  }

  if (willDrawUi) {
    onDrawUi->Execute(frame);
  }

  DrawingSubsystem::TransitionImage(*cmd, _swapchainImages[swapchainImageIndex],
                                    vk::ImageLayout::eUndefined,
                                    vk::ImageLayout::eTransferDstOptimal);

  // if(willDrawScenes) {
  //   if(auto sceneDrawn = Engine::Get()->GetScenes().at(0).Reserve()) {
  //     DrawingSubsystem::CopyImageToImage(*cmd, sceneDrawn->GetDrawer().Reserve()->GetRenderTarget().Reserve()->image,
  //                  _swapchainImages[swapchainImageIndex],
  //                  drawExtent, swapchainExtent);
  //   }
  // }

  if (willDrawUi) {
    if (auto widgetRoot = Engine::Get()->GetWidgetSubsystem().lock()->
                                         GetRoot(this->_window).lock()) {
      DrawingSubsystem::CopyImageToImage(
          *cmd, widgetRoot->GetRenderTarget().lock()->image,
          _swapchainImages[swapchainImageIndex],
          drawExtent, swapchainExtent);
    }
  }

  DrawingSubsystem::TransitionImage(*cmd, _swapchainImages[swapchainImageIndex],
                                    vk::ImageLayout::eTransferDstOptimal,
                                    vk::ImageLayout::ePresentSrcKHR);

  // Cant add commands anymore
  cmd->end();

  const auto cmdInfo = vk::CommandBufferSubmitInfo(*cmd, 0);

  const auto waitingInfo = vk::SemaphoreSubmitInfo(
      frame->GetSwapchainSemaphore(), 1,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput);
  const auto signalInfo = vk::SemaphoreSubmitInfo(
      frame->GetRenderSemaphore(), 1, vk::PipelineStageFlagBits2::eAllGraphics);

  const auto submitInfo = vk::SubmitInfo2({}, waitingInfo, cmdInfo,
                                          signalInfo);

  const auto renderSemaphore = frame->GetRenderSemaphore();
  const auto presentInfo = vk::PresentInfoKHR(renderSemaphore,
                                              _swapchain,
                                              swapchainImageIndex);

  try {
    _drawingSubsystem->SubmitAndPresent(frame, submitInfo, presentInfo);
  } catch (vk::OutOfDateKHRError &_) {
    /* Need to resize here */
    _isResizing = true;
    return;
  }

  _frameCount++;
}

void WindowDrawer::OnDestroy() {
  Object::OnDestroy();
  const auto instance = _drawingSubsystem->GetVulkanInstance();
  instance.destroySurfaceKHR(_surface);
}
}
