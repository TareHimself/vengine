﻿#pragma once
#include "Allocator.hpp"
#include "descriptors.hpp"
#include "vengine/types.hpp"
#include "vengine/containers/Array.hpp"
#include "vengine/containers/String.hpp"

#include <vk_mem_alloc.h>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>

namespace vengine::drawing {

#ifndef DRAWING_SERIALIZATION_OPS
#define DRAWING_SERIALIZATION_OPS
SIMPLE_BUFFER_SERIALIZER(Buffer,vk::Format);
SIMPLE_BUFFER_SERIALIZER(Buffer,vk::Filter);
#endif


enum class EMaterialPass : uint8_t {
  Opaque,
  Translucent,
  Transparent,
  MATERIAL_PASS_MAX
};
struct FrameData {
private:
  vk::Semaphore _swapchainSemaphore,_renderSemaphore;
  vk::Fence _renderFence;
  vk::CommandPool _cmdPool;
  vk::CommandBuffer _cmdBuffer;
  DescriptorAllocatorGrowable _frameDescriptors{};
public:
  CleanupQueue cleaner;
  vk::CommandBuffer * GetCmd();
  vk::CommandPool * GetCmdPool();
  DescriptorAllocatorGrowable * GetDescriptorAllocator();
  vk::Semaphore GetSwapchainSemaphore() const;
  vk::Semaphore GetRenderSemaphore() const;
  vk::Fence GetRenderFence() const;
  void SetSemaphores(const vk::Semaphore &swapchain, const vk::Semaphore &render);
  void SetRenderFence(vk::Fence renderFence);
  void SetCommandPool(vk::CommandPool pool);
  void SetCommandBuffer(vk::CommandBuffer buffer);
  
};

struct VmaAllocated {
  Allocation alloc;
};

struct AllocatedBuffer : VmaAllocated {
  vk::Buffer buffer = nullptr;
};

struct AllocatedImage :  VmaAllocated{
  vk::Image image = nullptr;
  vk::ImageView view = nullptr;
  vk::Extent3D extent;
  vk::Format format;
};

struct VertexInputDescription {
  Array<vk::VertexInputBindingDescription> bindings;
  Array<vk::VertexInputAttributeDescription> attributes;
}; 

struct Vertex {
  glm::vec4 location;
  glm::vec4 normal;
  glm::vec4 uv;

  // static VertexInputDescription getVertexDescription();
};

struct GpuMeshBuffers {
  AllocatedBuffer indexBuffer;
  AllocatedBuffer vertexBuffer;
  vk::DeviceAddress vertexBufferAddress;
};

struct SceneDrawPushConstants {
  glm::mat4 transformMatrix;
  vk::DeviceAddress vertexBuffer;
};

struct ComputePushConstants {
  float time;
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct ComputeEffect {
  std::string name;
  vk::Pipeline pipeline;
  vk::PipelineLayout layout;
  uint32_t size = sizeof(ComputePushConstants) + 12;
  uint32_t offset = 0;
  ComputePushConstants data;
};

struct ShaderResources {
  std::unordered_map<std::string,std::pair<uint32_t,uint32_t>> images;
  std::unordered_map<std::string,std::pair<uint32_t,uint32_t>> pushConstants;
  std::unordered_map<std::string,std::pair<uint32_t,uint32_t>> uniformBuffers;
  
};

enum EMaterialResourceType {
  Image,
  PushConstant,
  UniformBuffer
};

struct MaterialResourceInfo {
  EMaterialResourceType type;
  vk::ShaderStageFlagBits stages;
  uint32_t binding;
};

}