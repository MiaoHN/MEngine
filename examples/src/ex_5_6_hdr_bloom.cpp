// = LearnOpenGL 5.advanced_lighting/7.bloom (HDR + bloom)
//   source: LearnOpenGL/src/5.advanced_lighting/7.bloom/bloom.cpp
//
// LO-exact HDR + bloom port on the "blinn_lo" path + the engine bloom pass
// (brightness threshold > 1 luminance, Gaussian blur) and the LO tone map
// (1 - exp(-x * exposure) then gamma) via Scene::SetLoHdrTone.
//   - wood floor + a few container2 cubes (LO positions/rotations/scales)
//   - 4 point lights with HDR colors, NO ambient/specular, attenuation 1/d^2
//     (LO 7.bloom.fs); a small emissive cube marks each light
//   - LO camera (0,0,5) FOV 45, 4:3 window, black clear
#include <memory>

#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;
using MEngine::examples::PutAxis;

namespace {
std::shared_ptr<Scene> BuildHdrBloom() {
  auto s = std::make_shared<Scene>();

  // --- wood floor: LO's floor cube (translate (0,-1,0), scale (12.5,0.5,12.5)
  //     with a ±1 cube == engine unit cube scaled x2, so (25,1,25)).
  {
    Entity floor = s->CreateEntity("floor");
    auto &t      = floor.AddComponent<Transform>();
    t.translation = {0.0f, -1.0f, 0.0f};
    t.scale       = glm::vec3(25.0f, 1.0f, 25.0f);
    floor.AddComponent<MeshComponent>(Mesh::CreateCube(),
                                      examples::BlinnLoDiffuse("textures/wood.png", 32.0f, /*srgb*/ true));
  }

  // --- scenery container2 cubes (LO positions/rotations; engine scale = 2x LO
  //     scale because our cube is unit-sized while LO's renderCube is ±1).
  const auto crate = []() {
    return examples::BlinnLoDiffuse("textures/container2.png", 32.0f, /*srgb*/ true);
  };
  Put(*s, Mesh::CreateCube(), crate(), {0.0f, 1.5f, 0.0f}, 1.0f);      // LO scale .5
  Put(*s, Mesh::CreateCube(), crate(), {2.0f, 0.0f, 1.0f}, 1.0f);      // LO scale .5
  PutAxis(*s, Mesh::CreateCube(), crate(), {-1.0f, -1.0f, 2.0f}, glm::normalize(glm::vec3(1, 0, 1)), 60.0f, 2.0f);
  PutAxis(*s, Mesh::CreateCube(), crate(), {0.0f, 2.7f, 4.0f}, glm::normalize(glm::vec3(1, 0, 1)), 23.0f, 2.5f);
  PutAxis(*s, Mesh::CreateCube(), crate(), {-2.0f, 1.0f, -3.0f}, glm::normalize(glm::vec3(1, 0, 1)), 124.0f, 2.0f);
  Put(*s, Mesh::CreateCube(), crate(), {-3.0f, 0.0f, 0.0f}, 1.0f);     // LO scale .5

  // --- 4 point lights (LO 7.bloom.fs): HDR color, no ambient/specular,
  //     attenuation 1/d^2 (constant 0 / linear 0 / quadratic 1).
  const glm::vec3 light_pos[4] = {{0.0f, 0.5f, 1.5f}, {-4.0f, 0.5f, -3.0f}, {3.0f, 0.5f, 1.0f}, {-0.8f, 2.4f, -1.0f}};
  const glm::vec3 light_col[4] = {{5.0f, 5.0f, 5.0f}, {10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 15.0f}, {0.0f, 5.0f, 0.0f}};
  for (int i = 0; i < 4; ++i) {
    PointLight l;
    l.position       = light_pos[i];
    l.ambient        = glm::vec3(0.0f);
    l.diffuse        = light_col[i];
    l.specular       = glm::vec3(0.0f);
    l.lo_attenuation = true;
    l.constant       = 0.0f;   // LO: 1 / (distance^2)
    l.linear         = 0.0f;
    l.quadratic      = 1.0f;
    s->AddPointLight(l);
    // bright emissive light-source cube (LO lightColor box, scale .25 -> 0.5)
    Put(*s, Mesh::CreateCube(), examples::Unlit(light_col[i]), light_pos[i], 0.5f);
  }

  // LO 7.bloom has no sun; black clear; LO HDR tone + gamma, bloom on.
  examples::NoSun(*s);
  s->SetLoLighting(true);
  s->SetLinearOutput(false);
  s->SetLoHdrTone(true);
  s->SetSkyboxEnabled(false);
  s->SetBackgroundColor(glm::vec3(0.0f));
  s->SetIblIntensity(0.0f);
  s->SetExposure(1.0f);  // LO default exposure; sRGB albedo maps keep brightness in check
  s->SetTAAEnabled(false);
  s->SetSSAOEnabled(false);
  // LO adds the blurred bright buffer additively with NO extra god-rays veil.
  s->SetGodRaysStrength(0.0f);
  s->SetBloomEnabled(true);
  s->SetBloomThreshold(1.0f);
  s->SetBloomStrength(1.0f);  // LO: scene + bloomBlur additively
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildHdrBloom, "LO 5.6 hdr + bloom", {0, 0, 0}, 0.0f, 0.0f, 5.0f,
                                           45.0f});
}
