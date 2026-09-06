// = LearnOpenGL 2.lighting/3.1.materials
//   source: LearnOpenGL/src/2.lighting/3.1.materials/materials.cpp
//
// 1:1 port on the MEngine Blinn pipeline's LearnOpenGL-exact lighting mode:
// one coral cube whose ambient/diffuse/specular + shininess are separate
// material terms (Phong reflect specular). The light color in LO animates with
// sin(); we freeze it at the classic white look so it matches the tutorial's
// materials figure (dark-coral ambient side, bright diffuse, white highlight).
//   - one cube at the origin, material: ambient/diffuse (1,0.5,0.31),
//     specular (0.5,0.5,0.5), shininess 32
//   - white lamp cube at LO lightPos (1.2, 1.0, 2.0)
//   - light ambient 0.1 / diffuse 0.5 / specular 1.0 (no attenuation here)
//   - LO clear 0.1, raw linear output, camera (0,0,3) FOV 45, 4:3 window
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildMaterials() {
  auto s = std::make_shared<Scene>();

  // Coral material: LO material.ambient/diffuse (1,0.5,0.31) with a grey
  // specular (0.5,0.5,0.5) and shininess 32.
  Put(*s, Mesh::CreateCube(),
      examples::BlinnLo(glm::vec3(1.0f, 0.5f, 0.31f), glm::vec3(0.5f, 0.5f, 0.5f), 32.0f),
      {0.0f, 0.0f, 0.0f});

  const glm::vec3 light_pos(1.2f, 1.0f, 2.0f);
  examples::Lamp(*s, light_pos, glm::vec3(1.0f), 0.2f);

  PointLight l;
  l.position = light_pos;
  l.ambient  = glm::vec3(0.1f);
  l.diffuse  = glm::vec3(0.5f);
  l.specular = glm::vec3(1.0f);
  s->AddPointLight(l);

  // LO 3.1 has only this point light (no sun).
  examples::NoSun(*s);
  examples::LoScene(*s, glm::vec3(0.1f, 0.1f, 0.1f));
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildMaterials, "LO 2.3 materials", {0, 0, 0}, 0.0f, 0.0f, 3.0f,
                                           45.0f});
}
