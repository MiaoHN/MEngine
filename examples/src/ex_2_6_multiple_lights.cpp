// = LearnOpenGL 2.lighting/6.multiple_lights
//   source: LearnOpenGL/src/2.lighting/6.multiple_lights/multiple_lights.cpp
//
// 1:1 port on the MEngine Blinn pipeline's LearnOpenGL-exact lighting mode
// (Scene::SetLoLighting): per-light ambient/diffuse/specular + Phong reflect
// specular (no NdotL) + container2_specular per-pixel specular map + LO
// attenuation - matching 6.multiple_lights.fs term-for-term.
//   - 10 containers (container2 diffuse + container2_specular map), each
//     rotated around LO's axis (1,0.3,0.5) by 20deg*i
//   - 4 point lights at LO positions, LO c/l/q attenuation, ambient 0.05 /
//     diffuse 0.8 / specular 1.0
//   - dim directional light (ambient 0.05 / diffuse 0.4 / specular 0.5),
//     LO clear 0.1 background, raw linear output (LO writes untonemapped)
//   - LO camera: position (0,0,3), vertical FOV 45, 4:3 window
//   - the LO flashlight spot light (attached to the camera) is omitted because
//     the shared example host uses an orbit camera
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::PutAxis;

namespace {
std::shared_ptr<Scene> BuildMultipleLights() {
  auto s = std::make_shared<Scene>();

  // --- containers: LO cubePositions[10], rotation axis (1,0.3,0.5), 20*i
  const glm::vec3 cube_pos[10] = {
      {0.0f, 0.0f, 0.0f},     {2.0f, 5.0f, -15.0f},   {-1.5f, -2.2f, -2.5f}, {-3.8f, -2.0f, -12.3f},
      {2.4f, -0.4f, -3.5f},   {-1.7f, 3.0f, -7.5f},   {1.3f, -2.0f, -2.5f},  {1.5f, 2.0f, -2.5f},
      {1.5f, 0.2f, -1.5f},    {-1.3f, 1.0f, -1.5f},
  };
  const glm::vec3 axis = glm::vec3(1.0f, 0.3f, 0.5f);
  const auto crate = []() {
    return examples::BlinnLoTextured("textures/container2.png", "textures/container2_specular.png", 32.0f);
  };
  for (int i = 0; i < 10; ++i) {
    PutAxis(*s, Mesh::CreateCube(), crate(), cube_pos[i], axis, 20.0f * static_cast<float>(i));
  }

  // --- 4 point lights at LO positions: ambient 0.05 / diffuse 0.8 /
  //     specular 1.0, LO constant/linear/quadratic attenuation
  const glm::vec3 point_pos[4] = {
      {0.7f, 0.2f, 2.0f}, {2.3f, -3.3f, -4.0f}, {-4.0f, 2.0f, -12.0f}, {0.0f, 0.0f, -3.0f}};
  for (int i = 0; i < 4; ++i) {
    PointLight l;
    l.position      = point_pos[i];
    l.ambient       = glm::vec3(0.05f);
    l.diffuse       = glm::vec3(0.8f);
    l.specular      = glm::vec3(1.0f);
    l.lo_attenuation = true;
    l.constant      = 1.0f;
    l.linear        = 0.09f;
    l.quadratic     = 0.032f;
    s->AddPointLight(l);
    examples::Lamp(*s, point_pos[i], glm::vec3(1.0f), 0.2f);  // LO light_cube
  }

  // --- dim directional light (LO ambient 0.05 / diffuse 0.4 / specular 0.5)
  s->GetLight().direction = glm::normalize(glm::vec3(-0.2f, -1.0f, -0.3f));
  s->GetLight().ambient   = glm::vec3(0.05f);
  s->GetLight().diffuse   = glm::vec3(0.4f);
  s->GetLight().specular  = glm::vec3(0.5f);

  // LO clears to 0.1 and writes the raw result (no tone/gamma).
  examples::LoScene(*s, glm::vec3(0.1f, 0.1f, 0.1f));
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildMultipleLights, "LO 2.6 multiple_lights", {0, 0, 0}, 0.0f, 0.0f,
                                           3.0f, 45.0f});
}
