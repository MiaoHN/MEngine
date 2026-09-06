// = LearnOpenGL 2.lighting/6.multiple_lights
//   source: LearnOpenGL/src/2.lighting/6.multiple_lights/multiple_lights.cpp
//
// 1:1 port on the MEngine Blinn-Phong pipeline:
//   - 10 containers (container2 albedo) at LO positions, each rotated around
//     the LO axis (1,0.3,0.5) by 20deg*i (via Transform axis-angle support)
//   - 4 point lights at LO positions with LO constant/linear/quadratic
//     attenuation (lo_attenuation), white ~0.8 diffuse
//   - a dim directional light, small ambient, grey 0.1 clear background
//   - LO camera: position (0,0,3), vertical FOV 45, looking towards -Z
//   - the LO flashlight spot light (attached to the camera) is omitted because
//     the shared example host uses an orbit camera
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;
using MEngine::examples::PutAxis;

namespace {
std::shared_ptr<Scene> BuildMultipleLights() {
  auto s = std::make_shared<Scene>();

  // --- containers: LO cubePositions[10], rotation axis (1,0.3,0.5), angle 20*i
  const glm::vec3 cube_pos[10] = {
      {0.0f, 0.0f, 0.0f},     {2.0f, 5.0f, -15.0f},   {-1.5f, -2.2f, -2.5f}, {-3.8f, -2.0f, -12.3f},
      {2.4f, -0.4f, -3.5f},   {-1.7f, 3.0f, -7.5f},   {1.3f, -2.0f, -2.5f},  {1.5f, 2.0f, -2.5f},
      {1.5f, 0.2f, -1.5f},    {-1.3f, 1.0f, -1.5f},
  };
  const glm::vec3 axis  = glm::vec3(1.0f, 0.3f, 0.5f);
  const auto      crate = []() { return examples::BlinnTextured("textures/container2.png", 32.0f, 0.4f); };
  for (int i = 0; i < 10; ++i) {
    PutAxis(*s, Mesh::CreateCube(), crate(), cube_pos[i], axis, 20.0f * static_cast<float>(i));
  }

  // --- 4 point lights at LO positions, LO attenuation, white ~0.8 diffuse
  const glm::vec3 point_pos[4] = {
      {0.7f, 0.2f, 2.0f}, {2.3f, -3.3f, -4.0f}, {-4.0f, 2.0f, -12.0f}, {0.0f, 0.0f, -3.0f}};
  for (int i = 0; i < 4; ++i) {
    PointLight l;
    l.position      = point_pos[i];
    l.color         = glm::vec3(0.8f, 0.8f, 0.8f);
    l.intensity     = 1.0f;
    l.lo_attenuation = true;
    l.constant      = 1.0f;
    l.linear        = 0.09f;
    l.quadratic     = 0.032f;
    s->AddPointLight(l);
    // pure-white emissive light cube (like LO's light_cube shader)
    Put(*s, Mesh::CreateCube(), examples::Unlit(glm::vec3(1.0f)), point_pos[i], 0.2f);
  }

  // --- dim directional light (LO diffuse 0.4), near-black background, only a
  //     tiny ambient (= LO dirLight ambient ~0.05), and RAW linear output so
  //     the engine's ACES/gamma doesn't brighten it like LO's untonemapped view.
  examples::Sun(*s, {-0.2f, -1.0f, -0.3f}, glm::vec3(0.4f, 0.4f, 0.4f));
  examples::SolidBackground(*s, glm::vec3(0.008f, 0.008f, 0.008f), 0.15f);
  s->SetExposure(1.0f);
  s->SetLinearOutput(true);
  s->SetTAAEnabled(false);
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildMultipleLights, "LO 2.6 multiple_lights", {0, 0, 0}, 0.0f, 0.0f,
                                           3.0f, 45.0f});
}
