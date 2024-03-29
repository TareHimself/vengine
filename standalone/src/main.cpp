// #define VMA_DEBUG_LOG(format, ...) do { \
// printf(format, __VA_ARGS__); \
// printf("\n"); \
// } while(false)
#include "Test.hpp"
#include "vengine/io/io.hpp"
#include "vengine/scene/objects/PointLight.hpp"
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <vengine/Engine.hpp>
#include <vengine/scene/Scene.hpp>
#include <vengine/physics/rp3/RP3DScenePhysics.hpp>
using namespace vengine;

int main(int argc, char **argv) {

  try {

    io::setRawShadersPath(R"(D:\Github\vengine\shaders)");
    Engine::Get()->SetAppName("Test Application");
    
    const auto scene = Engine::Get()->CreateScene<scene::Scene>();
    auto t1 = scene.Reserve()->CreateSceneObject<TestGameObject>();
    
    Engine::Get()->Run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
//
//