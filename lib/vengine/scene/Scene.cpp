#include "vengine/drawing/scene/SceneDeferredDrawer.hpp"

#include <vengine/scene/Scene.hpp>
#include <vengine/scene/objects/DefaultCamera.hpp>
#include <vengine/Engine.hpp>
#include <vengine/input/InputSubsystem.hpp>
#include <vengine/physics/rp3/RP3DScenePhysics.hpp>
#include <vengine/drawing/scene/SceneDrawer.hpp>
#include <vengine/scene/objects/SceneObject.hpp>

namespace vengine::scene {

Engine *Scene::GetEngine() const {
  return GetOuter();
}

void Scene::Init(Engine * outer) {
  Object::Init(outer);
  _inputConsumer = CreateInputManager();
  _physics = CreatePhysics();
  _physics->Init(this);
  _drawer = CreateDrawer();
  _drawer->Init(this);
  
  _defaultViewTarget = InitSceneObject(CreateDefaultViewTarget());
  
  if(!_objectsPendingInit.empty()) {
    for(const auto &obj : _objectsPendingInit) {
      InitSceneObject(obj);
    }
    
    _objectsPendingInit.clear();
  }
}

void Scene::BeforeDestroy() {
  Object::BeforeDestroy();
  _sceneObjects.clear();

  _drawer.Clear();
  
  _physics.Clear();

  _inputConsumer.Clear();
}

void Scene::ReadFrom(Buffer &store) {
  uint64_t numObjects;
  store >> numObjects;

  for(auto i = 0; i < numObjects; i++) {
    String factoryId;
    uint64_t dataSize;
    store >> factoryId;
    store >> dataSize;
    log::engine->warn("Re-Creation of object not yet implemented",factoryId);
    store.Skip(dataSize);
    // if(HasObjectInFactory(factoryId)) {
    //   const auto createdObject = CreateObjectFromFactory<SceneObject>(factoryId);
    //   std::vector<char> objectData;
    //   objectData.resize(dataSize);
    //   store.Read(objectData.data(),dataSize);
    //   auto dataBuffer = MemoryBuffer(objectData);
    //   createdObject->ReadFrom(dataBuffer);
    //   InitSceneObject(createdObject);
    // }
    // else {
    //   
    // }
  }
}

void Scene::WriteTo(Buffer &store) {
  uint64_t numObjects = 0;
  MemoryBuffer objectsData;
  for(const auto &sceneObject : _sceneObjects) {
    if(!ShouldSerializeObject(sceneObject)) {
      continue;
    }

    if(const auto reflectedData = sceneObject->GetReflected()) {
      // Serialize Object [id,size,data]
      objectsData << reflectedData->GetName();
      MemoryBuffer objectData;
      sceneObject->WriteTo(objectData);
      objectsData << objectData.size();
      objectsData << objectData;

      numObjects++;
    }
    
    
  }
  store << numObjects;
  store << objectsData;
}

bool Scene::ShouldSerializeObject(const Managed<SceneObject> &object) {
  return object && object != _defaultViewTarget;
}

Array<Ref<SceneObject>> Scene::GetSceneObjects() const {
  Array<Ref<SceneObject>> result;
  for(auto &obj : _sceneObjects) {
    result.push(obj);
  }
  return result;
}

std::list<Ref<LightComponent>> Scene::GetSceneLights() const {
  return _lights;
}

Ref<drawing::SceneDrawer> Scene::GetDrawer() const {
  return _drawer;
}

Ref<physics::ScenePhysics> Scene::GetPhysics() const {
  return _physics;
}

Ref<input::SceneInputConsumer> Scene::GetInput() const {
  return _inputConsumer;
}

void Scene::RegisterLight(const Ref<LightComponent> &light) {
  _lights.push_back(light);
}

void Scene::Tick(float deltaTime) {
  if(_physics) {
    _physics->FixedUpdate(0.2f);
  }
  for(const auto &obj : _sceneObjects) {
    obj->Tick(deltaTime);
  }
}

Managed<physics::ScenePhysics> Scene::CreatePhysics() {
  return newManagedObject<physics::RP3DScenePhysics>();
}

Managed<drawing::SceneDrawer> Scene::CreateDrawer() {
  return newManagedObject<drawing::SceneDeferredDrawer>();
}

Managed<input::SceneInputConsumer> Scene::CreateInputManager() {
  return GetEngine()->GetInputSubsystem().Reserve()->Consume<input::SceneInputConsumer>().Reserve();
}

Managed<SceneObject> Scene::CreateDefaultViewTarget() {
  return newManagedObject<DefaultCamera>();
}

Ref<SceneObject> Scene::GetViewTarget() const {
  if(_viewTarget) {
    return _viewTarget;
  }

  return _defaultViewTarget;
}

Managed<SceneObject> Scene::InitSceneObject(const Managed<SceneObject> &object) {
  if (IsInitialized()) {
    _sceneObjects.push(object);
    object->Init(this);
  } else {
    _objectsPendingInit.push(object);
  }
  return object;
}
}
