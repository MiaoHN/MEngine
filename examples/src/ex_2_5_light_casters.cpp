// = LearnOpenGL 2.lighting/5.3.light_casters_spot (soft edges, LO 5.4)
//   source: LearnOpenGL/src/2.lighting/5.3.light_casters_spot/light_casters_spot.cpp
//
// Flashlight spot-light port on the LO-exact "blinn_lo" path: 10 wooden
// crates (container2 diffuse + container2_specular) at LO positions/rotations,
// lit ONLY by a spotlight anchored to the camera (the "flashlight"). Soft cone
// (cutOff 12.5 / outerCutOff 17.5, like LO 5.4) so the light circle falls off
// smoothly; the spot's ambient (0.1 * diffuse map) dimly lights everything
// outside the cone (LO's else branch), while diffuse/specular inside the cone
// get LO constant/linear/quadratic attenuation.
//   - LO camera at (0,0,3) looking -Z, FOV 45, 4:3 window
//   - right-drag orbits: the flashlight follows the camera eye/front each frame
#include <memory>

#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::PutAxis;

namespace {

/// @brief Replaces the scene's spot lights with one "flashlight" at `pos`
/// shining along `front` (LO 5.3's light = camera.Position / camera.Front).
void AddFlashlight(Scene &scene, const glm::vec3 &pos, const glm::vec3 &front) {
  scene.ClearSpotLights();
  SpotLight f;
  f.position     = pos;
  f.direction    = glm::normalize(front);
  f.ambient      = glm::vec3(0.1f);
  f.diffuse      = glm::vec3(0.8f);
  f.specular     = glm::vec3(1.0f);
  f.lo_attenuation = true;
  f.constant     = 1.0f;
  f.linear       = 0.09f;
  f.quadratic    = 0.032f;
  f.lo_flashlight = true;  // ambient everywhere, diffuse/spec inside the cone
  f.cutoff       = glm::cos(glm::radians(12.5f));
  f.outer_cutoff = glm::cos(glm::radians(17.5f));
  scene.AddSpotLight(f);
}

std::shared_ptr<Scene> BuildCasters() {
  auto s = std::make_shared<Scene>();

  // 10 containers exactly like LO (rotation axis (1,0.3,0.5), angle 20*i).
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

  // LO 5.3 lights the scene with the flashlight only (no sun).
  examples::NoSun(*s);
  examples::LoScene(*s, glm::vec3(0.1f, 0.1f, 0.1f));

  // Initial flashlight at LO's camera (0,0,3) shining down -Z at the crates.
  AddFlashlight(*s, glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, -1.0f));
  return s;
}

}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)

  MEngine::examples::ExampleApp::Setup setup;
  setup.build  = []() { return BuildCasters(); };
  setup.name   = "LO 2.5 light_casters (flashlight spot)";
  setup.target = {0, 0, 0};
  setup.yaw    = 0.0f;
  setup.pitch  = 0.0f;
  setup.dist   = 3.0f;
  setup.fov    = 45.0f;
  // Anchor the flashlight to the (orbit) camera every frame.
  setup.update = [](MEngine::Scene &scene, const glm::vec3 &eye, const glm::vec3 &front, float) {
    AddFlashlight(scene, eye, front);
  };
  return new MEngine::examples::ExampleApp(std::move(setup));
}
