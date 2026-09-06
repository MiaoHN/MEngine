// = LearnOpenGL 2.lighting/2.2.basic_lighting_specular
//   source: LearnOpenGL/src/2.lighting/2.2.basic_lighting_specular/basic_lighting_specular.cpp
//
// 1:1 port of LO's classic Phong specular demo on the MEngine Blinn pipeline's
// LearnOpenGL-exact lighting mode ("blinn_lo"): one coral cube, ambient 0.1,
// diffuse full, specular 0.5 (Phong reflect, no NdotL), shininess 32, one
// white point light.
//   - one cube at the origin, object color (1,0.5,0.31)
//   - the light-source cube ORBITS the cube (like LO's later demos) so you can
//     watch the specular highlight sweep across the surface
//   - light ambient 0.08 / diffuse 0.9 / specular 0.4 (slightly dimmed from
//     LO's 0.1/1.0/0.5 so the highlight reads a touch softer)
//   - LO clear 0.1, raw linear output, camera (0,0,3) FOV 45, 4:3 window
#include <cmath>
#include <memory>

#include "example_app.hpp"
#include "example_helpers.hpp"
#include "scene/component.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {

/// @brief Per-demo animation state shared by the builder + per-frame hook.
struct OrbitState {
  Entity  lamp;  // the emissive light-source cube (moves with the light)
  float   time  = 0.0f;
  float   radius = std::sqrt(1.2f * 1.2f + 2.0f * 2.0f);  // ~2.33, LO's dist
  float   height = 1.0f;                                   // keep LO's light y
  float   angle0 = std::atan2(2.0f, 1.2f);                 // start at LO (1.2,1,2)
  float   omega  = 0.45f;                                  // rad/s
};

std::shared_ptr<Scene> BuildBlinnLighting(const std::shared_ptr<OrbitState> &state) {
  auto s = std::make_shared<Scene>();

  // Coral cube at the origin. LO multiplies the whole result by objectColor,
  // so the specular sample is the object color too (tinted highlight).
  Put(*s, Mesh::CreateCube(),
      examples::BlinnLo(glm::vec3(1.0f, 0.5f, 0.31f), glm::vec3(1.0f, 0.5f, 0.31f), 32.0f),
      {0.0f, 0.0f, 0.0f});

  // Initial LO light position (1.2, 1.0, 2.0): the light-source cube entity is
  // kept (state->lamp) so the per-frame hook can move it around the cube.
  const glm::vec3 light_pos(std::cos(state->angle0) * state->radius, state->height,
                            std::sin(state->angle0) * state->radius);
  {
    Entity lamp = s->CreateEntity("lamp");
    auto &t     = lamp.AddComponent<Transform>();
    t.translation = light_pos;
    t.scale       = glm::vec3(0.2f);
    lamp.AddComponent<MeshComponent>(Mesh::CreateCube(), examples::Unlit(glm::vec3(1.0f)));
    state->lamp = lamp;
  }

  PointLight l;
  l.position = light_pos;
  l.ambient  = glm::vec3(0.08f);
  l.diffuse  = glm::vec3(0.9f);
  l.specular = glm::vec3(0.4f);
  s->AddPointLight(l);

  examples::LoScene(*s, glm::vec3(0.1f, 0.1f, 0.1f));
  return s;
}

/// @brief Orbits the point light + its lamp cube around the origin cube.
void OrbitLight(Scene &scene, const std::shared_ptr<OrbitState> &state, float dt) {
  state->time += dt;
  const float     a   = state->angle0 + state->time * state->omega;
  const glm::vec3 pos(std::cos(a) * state->radius, state->height, std::sin(a) * state->radius);

  // Refresh the single point light at the new position.
  scene.ClearPointLights();
  PointLight l;
  l.position = pos;
  l.ambient  = glm::vec3(0.08f);
  l.diffuse  = glm::vec3(0.9f);
  l.specular = glm::vec3(0.4f);
  scene.AddPointLight(l);

  // Drag the emissive lamp cube along.
  if (state->lamp.HasComponent<Transform>()) {
    state->lamp.GetComponent<Transform>().translation = pos;
  }
}

}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)

  auto state = std::make_shared<OrbitState>();
  MEngine::examples::ExampleApp::Setup setup;
  setup.build  = [state]() { return BuildBlinnLighting(state); };
  setup.name   = "LO 2.2 basic_lighting (Blinn)";
  setup.target = {0, 0, 0};
  setup.yaw    = 0.0f;
  setup.pitch  = 0.0f;
  setup.dist   = 3.0f;
  setup.fov    = 45.0f;
  setup.update = [state](MEngine::Scene &scene, float dt) { OrbitLight(scene, state, dt); };
  return new MEngine::examples::ExampleApp(std::move(setup));
}
