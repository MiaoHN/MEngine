// = LearnOpenGL 2.lighting/2.2.basic_lighting_specular
//   source: LearnOpenGL/src/2.lighting/2.2.basic_lighting_specular/basic_lighting_specular.cpp
//
// 1:1 port of LO's classic Phong specular demo on the MEngine Blinn pipeline's
// LearnOpenGL-exact lighting mode: one coral cube, ambient 0.1, diffuse full,
// specular 0.5 (Phong reflect, no NdotL), shininess 32, one white point light.
//   - one cube at the origin, object color (1,0.5,0.31)
//   - white lamp cube at LO lightPos (1.2, 1.0, 2.0)
//   - light ambient 0.1 / diffuse 1.0 / specular 0.5 (no attenuation here)
//   - LO clear 0.1, raw linear output, camera (0,0,3) FOV 45, 4:3 window
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildBlinnLighting() {
  auto s = std::make_shared<Scene>();

  // Coral cube at the origin. LO multiplies the whole result by objectColor,
  // so the specular sample is the object color too (tinted highlight).
  Put(*s, Mesh::CreateCube(),
      examples::BlinnLo(glm::vec3(1.0f, 0.5f, 0.31f), glm::vec3(1.0f, 0.5f, 0.31f), 32.0f),
      {0.0f, 0.0f, 0.0f});

  const glm::vec3 light_pos(1.2f, 1.0f, 2.0f);
  examples::Lamp(*s, light_pos, glm::vec3(1.0f), 0.2f);

  PointLight l;
  l.position = light_pos;
  l.ambient  = glm::vec3(0.1f);
  l.diffuse  = glm::vec3(1.0f);
  l.specular = glm::vec3(0.5f);
  s->AddPointLight(l);

  examples::LoScene(*s, glm::vec3(0.1f, 0.1f, 0.1f));
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildBlinnLighting, "LO 2.2 basic_lighting (Blinn)", {0, 0, 0}, 0.0f,
                                           0.0f, 3.0f, 45.0f});
}
