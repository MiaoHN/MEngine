// = LearnOpenGL 5.advanced_lighting/8.1.deferred_shading
//   source: LearnOpenGL/src/5.advanced_lighting/8.1.deferred_shading/deferred_shading.cpp
//
// NOT a deferred renderer: the engine renders forward (per-object batching).
// This is a FORWARD scene-equivalent that keeps LO 8.1's exact scene data - a
// 3x3 grid of backpacks at LO positions and all 32 of LO's srand(13) random
// colored point lights (the shared "blinn_lo" forward shader was raised to 32
// point lights for this; it has no shadow-sampler arrays) - re-lit with LO's
// exact per-light Blinn-Phong math and 1/(1+0.7d+1.8d^2) attenuation, so the
// look matches the LO demo as closely as a forward pipeline allows.
//
//   - 9 backpacks (backpack.obj, scale .5) at LO's grid positions
//   - all 32 LO colored point lights + a matching emissive cube each
//   - no ambient / no sun (LO deferred has none) -> unlit parts stay black
//   - camera (0,0,5) FOV 45, 4:3 window, black clear
#include <cstdlib>
#include <memory>

#include "example_app.hpp"
#include "example_helpers.hpp"
#include "render/model_loader.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {

std::shared_ptr<Scene> BuildDeferred() {
  auto s = std::make_shared<Scene>();

  // --- the 9 backpacks, exactly LO 8.1's grid (x,z in {-3,0,3}, y = -0.5,
  //     scale 0.5). One shared mesh instance is reused by all 9 entities.
  // LoadObj opens a raw path (no AssetManager prefix), so use the assets/ root
  // relative to the working directory (the per-exe assets copy).
  Ref<Mesh> backpack_mesh = ModelLoader::LoadObj("assets/models/backpack/backpack.obj");
  // LO deferred stores the raw (sRGB) diffuse bytes in the G-buffer with no
  // linearization, so keep the map raw here (srgb = false) to match its look.
  Ref<Material> backpack_mat =
      examples::BlinnLoDiffuse("models/backpack/diffuse.jpg", 32.0f, /*srgb*/ false);
  const glm::vec3 grid[9] = {{-3.0f, 0.0f, -3.0f}, {0.0f, 0.0f, -3.0f}, {3.0f, 0.0f, -3.0f},
                             {-3.0f, 0.0f, 0.0f},  {0.0f, 0.0f, 0.0f},  {3.0f, 0.0f, 0.0f},
                             {-3.0f, 0.0f, 3.0f},  {0.0f, 0.0f, 3.0f},  {3.0f, 0.0f, 3.0f}};
  for (const glm::vec3 &p : grid) {
    Entity e = s->CreateEntity("backpack");
    auto &t  = e.AddComponent<Transform>();
    t.translation = p + glm::vec3(0.0f, -0.5f, 0.0f);
    t.scale       = glm::vec3(0.5f);
    e.AddComponent<MeshComponent>(backpack_mesh, backpack_mat);
  }

  // --- LO's 32 srand(13) random colored point lights (deferred demo). The
  //     forward "blinn_lo" path was raised to 32 lights for exactly this demo
  //     (it has no shadow-sampler arrays, unlike blinn/pbr), so we run LO's
  //     full set with LO's exact positions/colors and LO's deferred_shading
  //     attenuation (constant 1 / linear 0.7 / quad 1.8).
  std::srand(13);
  for (int i = 0; i < 32; ++i) {
    const float xPos = static_cast<float>(((std::rand() % 100) / 100.0f) * 6.0f - 3.0f);
    const float yPos = static_cast<float>(((std::rand() % 100) / 100.0f) * 6.0f - 4.0f);
    const float zPos = static_cast<float>(((std::rand() % 100) / 100.0f) * 6.0f - 3.0f);
    const float rCol = static_cast<float>(((std::rand() % 100) / 200.0f) + 0.5f);
    const float gCol = static_cast<float>(((std::rand() % 100) / 200.0f) + 0.5f);
    const float bCol = static_cast<float>(((std::rand() % 100) / 200.0f) + 0.5f);

    const glm::vec3 pos(xPos, yPos, zPos);
    const glm::vec3 color(rCol, gCol, bCol);

    PointLight l;
    l.position       = pos;
    l.ambient        = glm::vec3(0.0f);  // LO deferred has no ambient term
    l.diffuse        = color;
    l.specular       = color;
    l.lo_attenuation = true;
    l.constant       = 1.0f;   // LO: 1 / (1 + 0.7*d + 1.8*d^2)
    l.linear         = 0.7f;
    l.quadratic      = 1.8f;
    s->AddPointLight(l);

    // Emissive light-source cube (LO renderCube scale .125 -> engine cube .25).
    Put(*s, Mesh::CreateCube(), examples::Unlit(color), pos, 0.25f);
  }

  // LO deferred has a hard-coded `Diffuse * 0.1` ambient (no directional sun).
  // Reproduce it with the engine's directional light acting as pure ambient:
  // ambient 0.1 x albedo, no diffuse/specular, no shadow.
  {
    auto &l = s->GetLight();
    l.ambient  = glm::vec3(0.1f);
    l.diffuse  = glm::vec3(0.0f);
    l.specular = glm::vec3(0.0f);
    l.color    = glm::vec3(1.0f);
  }
  examples::LoScene(*s, glm::vec3(0.0f));
  return s;
}

}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildDeferred, "LO 5.8 deferred_shading (forward eq.)",
                                           {0, 0.0f, 0}, 0.0f, 0.0f, 5.0f, 45.0f});
}
