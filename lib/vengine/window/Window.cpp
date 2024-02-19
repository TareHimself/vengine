#include "vengine/window/Window.hpp"

#include "vengine/assets/Asset.hpp"

namespace vengine::window {
Window::Window(GLFWwindow * window,uint64_t id) {
  _window = window;
  glfwSetWindowUserPointer(_window,this);
  _id = id;
}

uint64_t Window::GetId() const {
  return _id;
}

void Window::Init(WindowManager *outer) {
  Object::Init(outer);
  if(_window != nullptr) {
    glfwSetKeyCallback(_window,[](GLFWwindow* window, int key, int scancode, int action, int mods) {
      const auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self->HandleKey(key,scancode,action,mods);
    });

    glfwSetCursorPosCallback(_window,[](GLFWwindow* window, double x, double y) {
      const auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self->HandleMouseMove(x,y);
    });

    glfwSetMouseButtonCallback(_window,[](GLFWwindow* window, int button, int action, int mods) {
      const auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self->HandleMouseButton(button,action,mods);
    });

    glfwSetWindowFocusCallback(_window,[](GLFWwindow* window, int focused) {
      const auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      const auto newFocus = static_cast<bool>(focused);
      self->onFocusChanged(self,newFocus);
      getManager()->onWindowFocusChanged(self,newFocus);
    });

    glfwSetScrollCallback(_window,[](GLFWwindow* window, const double xoffset, const double yoffset) {
      const auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self->HandleScroll(xoffset,yoffset);
    });

    glfwSetWindowSizeCallback(_window,[](GLFWwindow* window, const int width,const int height) {
      const auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self->HandleResize(width,height);
    });

    glfwSetFramebufferSizeCallback(_window,[](GLFWwindow* window, const int width,const int height) {
      const auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self->HandleResize(width,height);
    });

    glfwSetWindowCloseCallback(_window,[](GLFWwindow* window) {
      const auto self = static_cast<Window*>(glfwGetWindowUserPointer(window));
      self->onCloseRequested(self);
      getManager()->onWindowCloseRequested(self);
    });
  }
}

GLFWwindow * Window::GetRaw() const {
  return _window;
}

vk::SurfaceKHR Window::CreateSurface(const vk::Instance &instance) const {
  VkSurfaceKHR rawSurface;
  const VkInstance rawInstance = instance;
  glfwCreateWindowSurface(rawInstance,_window,nullptr,&rawSurface);
  return rawSurface;
}

void Window::SetCursorMode(ECursorMode mode) const {
  glfwSetInputMode(_window,GLFW_CURSOR,mode);
}

glm::uvec2 Window::GetSize() const {
  int width,height = 0;

  glfwGetWindowSize(_window,&width,&height);
  
  return {width,height};
}

glm::uvec2 Window::GetPixelSize() const {
  int width,height = 0;

  glfwGetFramebufferSize(_window,&width,&height);
  
  return {width,height};
}

glm::dvec2 Window::GetMousePosition() const {
  double x,y = 0;
  glfwGetCursorPos(_window,&x,&y);

  return {x,y};
}

Ref<Window> Window::CreateChild(const int width, const int height,const std::string &name) {
  return getManager()->Create(width,height,name,this);
}

void Window::SetMousePosition(const glm::dvec2 &position) const {
  glfwSetCursorPos(_window,position.x,position.y);
}

bool Window::CloseRequested() const {
  return glfwWindowShouldClose(_window);
}

void Window::HandleKey(int key, int scancode, int action, int mods){
  if(action == GLFW_PRESS) {
    onKeyDown(std::make_shared<KeyEvent>(this,static_cast<EKey>(key)));
    getManager()->onWindowKeyDown(std::make_shared<KeyEvent>(this,static_cast<EKey>(key)));
  }

  if(action == GLFW_RELEASE){
    onKeyUp(std::make_shared<KeyEvent>(this,static_cast<EKey>(key)));
    getManager()->onWindowKeyUp(std::make_shared<KeyEvent>(this,static_cast<EKey>(key)));
  }
  
}

void Window::HandleMouseMove(const double x, const double y){
  onMouseMoved(std::make_shared<MouseMovedEvent>(this,x,y));
  getManager()->onWindowMouseMoved(std::make_shared<MouseMovedEvent>(this,x,y));
}

void Window::HandleMouseButton(int button, const int action, int mods){
  const auto mousePos = GetMousePosition();
  
  auto buttonEnum = static_cast<EMouseButton>(button);
  
  if(action == GLFW_PRESS) {
    onMouseDown(std::make_shared<MouseButtonEvent>(this,buttonEnum,mousePos.x,mousePos.y));
    getManager()->onWindowMouseDown(std::make_shared<MouseButtonEvent>(this,buttonEnum,mousePos.x,mousePos.y));
  }
  if(action == GLFW_RELEASE){
    onMouseUp(std::make_shared<MouseButtonEvent>(this,buttonEnum,mousePos.x,mousePos.y));
    getManager()->onWindowMouseUp(std::make_shared<MouseButtonEvent>(this,buttonEnum,mousePos.x,mousePos.y));
  }
}

void Window::HandleScroll(double xOffset, double yOffset) {
  auto position = GetMousePosition();
  onScroll(std::make_shared<ScrollEvent>(this,position.x,position.y,xOffset,yOffset));
  getManager()->onWindowScroll(std::make_shared<ScrollEvent>(this,position.x,position.y,xOffset,yOffset));
}

void Window::HandleResize(const int width, const int height) {
  onResize(this);
  getManager()->onWindowResize(this);
}

bool Window::IsFocused() const {
  return static_cast<bool>(glfwGetWindowAttrib(_window, GLFW_FOCUSED));
}

void Window::SetOwner(Window *owner) {
  _owner = owner;
  AddCleanup(_owner->onDestroyed,_owner->onDestroyed.Bind([this] {
    getManager()->Destroy(GetId());
  }));
}

Window * Window::GetOwner() const {
  return _owner;
}

void Window::BeforeDestroy() {
  Object::BeforeDestroy();
  glfwDestroyWindow(_window);
  _window = nullptr;
}

void WindowManager::Start() {
  glfwInit();
}

void WindowManager::Stop(){
  _windows.clear();
  glfwTerminate();
}

Ref<Window> WindowManager::Create(int width, int height,
    const std::string &name,Window *owner) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  
  if(const auto window = CreateRaw(width,height,name,nullptr)) {
    _windows.emplace(window->GetId(),window);
    window->Init(this);
    if(owner) {
      window->SetOwner(owner);
    }
    onWindowCreated(window);
    return window;
  }
  return {};
}

Managed<Window> WindowManager::CreateRaw(int width, int height,
    const std::string &name, const Window *owner) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  
  if(const auto window = glfwCreateWindow(width,height,name.c_str(),nullptr,owner == nullptr ? nullptr : owner->GetRaw())) {
    uint64_t windowId = _ids++;
    
    if(auto mObject = newManagedObject<Window>(window,windowId)) {
      Ref<Window> weakRef = mObject;
      mObject->onDestroyed.Bind([weakRef] {
        getManager()->onWindowDestroyed(weakRef);
      });
      
      return mObject;
    }
  }
  return {};
}

Ref<Window> WindowManager::Find(uint64_t id) {
  if(_windows.contains(id)) {
    return _windows[id];
  }

  return {};
}

void WindowManager::Destroy(uint64_t id) {
  if(_windows.contains(id)) {
    _windows.erase(_windows.find(id));
  }
}

void WindowManager::Poll() {
  glfwPollEvents();
}

Array<Ref<Window>> WindowManager::GetWindows() const {
  Array<Ref<Window>> windows;
  for(const auto &val : _windows | std::views::values) {
    windows.push(val);
  }
  return windows;
}

std::shared_ptr<WindowManager> getManager() {
  static auto instance = std::make_shared<WindowManager>();
  return instance;
}

std::pair<uint32_t, const char **> getExtensions() {
  uint32_t numExtensions = 0;
  auto data = glfwGetRequiredInstanceExtensions(&numExtensions);

  return std::pair{numExtensions,data};
}

void init() {
  getManager()->Start();
}

void poll() {
  getManager()->Poll();
}

void terminate() {
  getManager()->Stop();
}

void destroy(uint64_t id) {
  getManager()->Destroy(id);
}

Ref<Window> create(int width, int height, const std::string &name,
                   const Ref<Window>& owner) {
  return getManager()->Create(width,height,name,owner.Reserve().Get());
}
}
