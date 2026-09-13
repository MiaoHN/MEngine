// = LearnOpenGL 2.lighting/4.2.lighting_maps_specular_map
//   source: LearnOpenGL/src/2.lighting/4.2.lighting_maps_specular_map/lighting_maps_specular.cpp
//
// 1:1 port on the MEngine Blinn pipeline's LearnOpenGL-exact lighting mode:
// a single container2 crate whose *specular* comes from container2_specular
// (per-pixel metal edges), lit by one white point light. Matches
// 4.2.lighting_maps.fs exactly (Phong reflect, no attenuation in this demo).
//   - one cube at the origin (container2 diffuse + specular map, shininess 64)
//   - white lamp cube at LO lightPos (1.2, 1.0, 2.0)
//   - point light ambient 0.2 / diffuse 0.5 / specular 1.0
//   - LO clear 0.1, raw linear output, camera (0,0,3) FOV 45, 4:3 window
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildLightingMaps() {
  auto s = std::make_shared<Scene>();

  // One crate at the origin, diffuse + specular maps, LO shininess 64.
  Put(*s, Mesh::CreateCube(),
      examples::BlinnLoTextured("textures/container2.png", "textures/container2_specular.png", 64.0f),
      {0.0f, 0.0f, 0.0f});

  // The white lamp cube (LO light_cube) at the light position.
  const glm::vec3 light_pos(1.2f, 1.0f, 2.0f);
  examples::Lamp(*s, light_pos, glm::vec3(1.0f), 0.2f);

  PointLight l;
  l.position = light_pos;
  l.ambient  = glm::vec3(0.2f);
  l.diffuse  = glm::vec3(0.5f);
  l.specular = glm::vec3(1.0f);
  s->AddPointLight(l);

  // LO 4.2 has only this point light (no sun).
  examples::NoSun(*s);
  examples::LoScene(*s, glm::vec3(0.1f, 0.1f, 0.1f));
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildLightingMaps, "LO 2.4 lighting_maps", {0, 0, 0}, 0.0f, 0.0f,
                                           3.0f, 45.0f});
}
