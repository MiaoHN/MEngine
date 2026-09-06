// = LearnOpenGL 5.advanced_lighting/4.normal_mapping
//   source: LearnOpenGL/src/5.advanced_lighting/4.normal_mapping/normal_mapping.cpp
//
// 1:1 port on the MEngine "blinn_lo" LO-exact path with normal mapping (the
// shader builds the TBN from screen-space derivatives, like the pbr path):
// a 2x2 brick wall whose surface normal is perturbed by brickwall_normal.jpg,
// lit by one white point light. LO uses a Blinn-Phong halfway specular here,
// so the scene turns on Scene::SetLoBlinnSpec(true).
//   - brick wall (brickwall.jpg + brickwall_normal.jpg), double-sided, slowly
//     tumbling so the relief reads from changing angles (LO rotates it too)
//   - light at LO lightPos (0.5, 1.0, 0.3), NO attenuation (LO 4 has none):
//     ambient 0.1 / diffuse 1.0 / specular (0.2 grey in the material)
//   - LO clear 0.1, raw linear output, camera (0,0,3) FOV 45, 4:3 window
#include <memory>

#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {

/// @brief Demo state shared by the builder + per-frame hook (the tumbling wall).
struct State {
  Entity wall;
  float  tumble = 0.0f;  // degrees, tumbled about the world X axis (-10/s like LO)
};

std::shared_ptr<Scene> BuildNormalMapping(const std::shared_ptr<State> &state) {
  auto s = std::make_shared<Scene>();

  // LO's 2x2 quad in the XY plane, brickwall mapped once (uv 0..1).
  Ref<Material> wall_mat = examples::BlinnLoNormalMapped("textures/brickwall.jpg",
                                                         "textures/brickwall_normal.jpg", 32.0f, 0.2f);
  {
    Entity wall = s->CreateEntity("brick_wall");
    auto &t     = wall.AddComponent<Transform>();
    t.translation = {0.0f, 0.0f, 0.0f};
    t.scale       = glm::vec3(1.0f);
    wall.AddComponent<MeshComponent>(examples::TiledWall(2.0f, 2.0f, 1.0f), wall_mat);
    state->wall = wall;
  }

  const glm::vec3 light_pos(0.5f, 1.0f, 0.3f);
  examples::Lamp(*s, light_pos, glm::vec3(1.0f), 0.1f);  // LO's small light quad

  PointLight l;
  l.position = light_pos;
  l.ambient  = glm::vec3(0.1f);
  l.diffuse  = glm::vec3(1.0f);
  l.specular = glm::vec3(1.0f);  // specular strength 0.2 lives in the material
  s->AddPointLight(l);

  // LO 4 has only this one point light (no sun); normal mapping uses Blinn.
  examples::NoSun(*s);
  s->SetLoBlinnSpec(true);
  examples::LoScene(*s, glm::vec3(0.1f, 0.1f, 0.1f));
  return s;
}

}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)

  auto state = std::make_shared<State>();
  MEngine::examples::ExampleApp::Setup setup;
  setup.build  = [state]() { return BuildNormalMapping(state); };
  setup.name   = "LO 5.4 normal_mapping";
  setup.target = {0, 0, 0};
  setup.yaw    = 0.0f;
  setup.pitch  = 0.0f;
  setup.dist   = 3.0f;
  setup.fov    = 45.0f;
  setup.update = [state](MEngine::Scene &, const glm::vec3 &, const glm::vec3 &, float dt) {
    // Slowly tumble the wall like LO (single-axis euler = smooth, no flips).
    state->tumble -= 10.0f * dt;
    if (state->wall.HasComponent<Transform>()) {
      auto &t  = state->wall.GetComponent<Transform>();
      t.rotation.x = state->tumble;
    }
  };
  return new MEngine::examples::ExampleApp(std::move(setup));
}
