// = LearnOpenGL 5.advanced_lighting/9.ssao (screen-space ambient occlusion)
//   source: LearnOpenGL/src/5.advanced_lighting/9.ssao/ssao.cpp
//
// This is NOT a 1:1 LO port: LO 9.ssao runs its own deferred-style G-buffer +
// SSAO chain (9.ssao_geometry/9.ssao/9.ssao_blur/9.ssao_lighting) that the
// engine's forward renderer cannot reproduce internally.  Instead this demo
// exercises the ENGINE's real SSAO pass (view-space G-buffer -> hemisphere
// kernel -> blur) on a scene built in the LO 9.ssao spirit (a floor + crates
// + the backpack model, one overhead light) so you can see the ambient
// occlusion darken the creases/contacts.  Space toggles SSAO on/off.
//
//   - classic engine Blinn-Phong path (SSAO is applied to the full result there)
//   - floor + a cluster of crates + the LO backpack, close together so the
//     SSAO contact darkening is obvious
//   - one bright overhead point light (LO 9.ssao uses a single light at
//     (2,4,-2)) + a dim warm fill; LO-style dark clear
//   - camera (0,0,5) FOV 45, 4:3 window, LO 9.ssao's starting pose
#include <GLFW/glfw3.h>

#include <memory>

#include "core/input.hpp"
#include "example_app.hpp"
#include "example_helpers.hpp"
#include "render/model_loader.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {

/// @brief Shared per-demo state (the update hook toggles SSAO with Space).
struct SSAOState {
  bool    ssao_on   = true;  // starts ON; Space toggles (see ToggleSSAO)
  bool    edge_held = false;
};

/// @brief Classic Blinn material (not the LO-exact blinn_lo path - the classic
/// pipeline applies the engine's SSAO term to the whole lighting result).
Ref<Material> BlinnTex(const std::string &albedo_path, float shininess = 32.0f, float specular = 0.5f) {
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(AssetManager::Instance().GetShader("blinn"));
  m->SetAlbedoMap(AssetManager::Instance().GetTexture(albedo_path));
  m->SetSpecularFactor(specular);
  m->SetShininess(shininess);
  return m;
}

std::shared_ptr<Scene> BuildSSAO(const std::shared_ptr<SSAOState> &state) {
  auto s = std::make_shared<Scene>();

  // --- wooden floor (top at y = 0). The crates + backpack sit on it, which is
  //     where the SSAO contact darkening shows up.
  {
    Entity floor = s->CreateEntity("floor");
    auto &t      = floor.AddComponent<Transform>();
    t.translation = {0.0f, -0.25f, 0.0f};
    t.scale       = glm::vec3(14.0f, 0.5f, 14.0f);
    floor.AddComponent<MeshComponent>(Mesh::CreateCube(),
                                      examples::Blinn(glm::vec3(0.45f, 0.32f, 0.20f), 16.0f, 0.3f));
  }

  // --- the LO backpack on the floor (used by LO 9.ssao's own demo).
  // LoadObj opens a raw path (no AssetManager prefix), so use the assets/ root
  // relative to the working directory (the per-exe assets copy).
  Ref<Mesh>     backpack_mesh = ModelLoader::LoadObj("assets/models/backpack/backpack.obj");
  Ref<Material> backpack_mat  = BlinnTex("models/backpack/diffuse.jpg", 64.0f, 0.4f);
  Put(*s, backpack_mesh, backpack_mat, {1.1f, 1.0f, 0.9f}, 1.0f);

  // --- a cluster of container2 crates so there are lots of floor/crate and
  //     crate/crate contacts for the AO to bite into.
  const auto crate = []() { return BlinnTex("textures/container2.png", 32.0f, 0.5f); };
  Put(*s, Mesh::CreateCube(), crate(), {-1.9f, 0.5f, -1.6f}, 1.0f);
  Put(*s, Mesh::CreateCube(), crate(), {-0.8f, 0.5f, -1.6f}, 1.0f);
  Put(*s, Mesh::CreateCube(), crate(), {0.2f, 0.5f, -1.4f}, 1.0f);
  Put(*s, Mesh::CreateCube(), crate(), {-0.4f, 0.5f, -0.3f}, 1.0f);
  Put(*s, Mesh::CreateCube(), crate(), {1.0f, 0.5f, -1.4f}, 1.0f);
  Put(*s, Mesh::CreateCube(), crate(), {2.0f, 0.5f, -0.6f}, 1.0f);
  Put(*s, Mesh::CreateCube(), crate(), {0.9f, 1.5f, -1.4f}, 1.0f);  // stacked second row
  Put(*s, Mesh::CreateCube(), crate(), {2.6f, 1.5f, -1.2f}, 1.0f);

  // --- lights: one bright overhead point light (LO 9.ssao: (2,4,-2)) + a dim
  //     warm fill so the back sides aren't pure black.
  PointLight key;
  key.position  = {2.0f, 4.0f, -2.0f};
  key.color     = {1.0f, 1.0f, 1.0f};
  key.intensity = 4.0f;
  key.radius    = 20.0f;
  s->AddPointLight(key);

  PointLight fill;
  fill.position  = {-3.0f, 2.0f, 3.0f};
  fill.color     = {1.0f, 0.85f, 0.65f};
  fill.intensity = 1.2f;
  fill.radius    = 25.0f;
  s->AddPointLight(fill);

  // Dark studio look: no skybox (blinn needs no IBL env), plain dark clear,
  // classic engine post-processing but with bloom / god-rays / TAA off so the
  // capture is stable. SSAO starts ON (Space toggles it).
  examples::SolidBackground(*s, glm::vec3(0.04f, 0.04f, 0.04f), 0.35f);
  s->SetBloomEnabled(false);
  s->SetGodRaysStrength(0.0f);
  s->SetTAAEnabled(false);
  s->SetSSAOEnabled(state->ssao_on);
  return s;
}

/// @brief Space toggles the engine's SSAO pass so you can compare on/off.
void ToggleSSAO(Scene &scene, const std::shared_ptr<SSAOState> &state, float) {
  const bool down = Input::IsKeyPressed(GLFW_KEY_SPACE);
  if (down && !state->edge_held) {
    state->edge_held = true;
    state->ssao_on   = !state->ssao_on;
    scene.SetSSAOEnabled(state->ssao_on);
  } else if (!down) {
    state->edge_held = false;
  }
}

}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)

  auto state = std::make_shared<SSAOState>();
  MEngine::examples::ExampleApp::Setup setup;
  setup.build  = [state]() { return BuildSSAO(state); };
  setup.name   = "LO 5.9 ssao (engine SSAO, Space toggles)";
  setup.target = {0, 0.6f, 0};
  setup.yaw    = 0.0f;
  setup.pitch  = 3.0f;
  setup.dist   = 5.0f;
  setup.fov    = 45.0f;
  setup.update = [state](MEngine::Scene &scene, const glm::vec3 &, const glm::vec3 &, float dt) {
    ToggleSSAO(scene, state, dt);
  };
  return new MEngine::examples::ExampleApp(std::move(setup));
}
