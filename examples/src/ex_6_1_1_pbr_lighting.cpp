// = LearnOpenGL 6.pbr/1.1.lighting (PBR direct lighting, untextured)
//   source: LearnOpenGL/src/6.pbr/1.1.lighting/lighting.cpp
//
// 1:1 port of LO's PBR lighting demo on the ENGINE's own PBR path (GGX
// cook-torrance): a 7x7 grid of spheres whose metallic = row/7 and roughness
// = clamp(col/7, 0.05, 1), red albedo 0.5, lit by 4 white HDR point lights at
// (+-10, +-10, 10) with intensity 300 and LO's 1/d^2 attenuation.
//   - engine PBR with IBL off (ibl_intensity 0), skybox off, LO clear 0.1;
//     composite uses the engine's ACES tone + gamma (LO 1.1.fs uses Reinhard)
//   - LO camera (0,0,3) FOV 45; orbit start pulled back to frame the whole
//     7x7 matrix, 800x600 4:3 window
#include <memory>

#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildPbrLighting() {
  auto s = std::make_shared<Scene>();

  // --- the 7x7 PBR sphere matrix (LO layout: (col-3, row-3) * 2.5, radius 1).
  Ref<Mesh> sphere = Mesh::CreateSphere(1.0f, 64);
  const int rows    = 7;
  const int columns = 7;
  const float spacing = 2.5f;
  for (int row = 0; row < rows; ++row) {
    const float metallic = static_cast<float>(row) / static_cast<float>(rows);
    for (int col = 0; col < columns; ++col) {
      const float roughness = std::clamp(static_cast<float>(col) / static_cast<float>(columns), 0.05f, 1.0f);
      const glm::vec3 pos(static_cast<float>(col - columns / 2) * spacing,
                          static_cast<float>(row - rows / 2) * spacing, 0.0f);
      Put(*s, sphere, examples::Pbr(glm::vec3(0.5f, 0.0f, 0.0f), metallic, roughness), pos, 1.0f);
    }
  }

  // --- 4 white HDR point lights, LO 1.1 (color 300, 1/d^2).
  const glm::vec3 lp[4] = {{-10.0f, 10.0f, 10.0f}, {10.0f, 10.0f, 10.0f},
                           {-10.0f, -10.0f, 10.0f}, {10.0f, -10.0f, 10.0f}};
  for (const glm::vec3 &p : lp) {
    PointLight l;
    l.position       = p;
    l.color          = glm::vec3(300.0f);  // pbr: radiance = color * intensity
    l.ambient        = glm::vec3(0.0f);
    l.diffuse        = glm::vec3(300.0f);
    l.specular       = glm::vec3(300.0f);
    l.lo_attenuation = true;
    l.constant       = 0.0f;  // LO 1.1.pbr.fs: 1 / d^2
    l.linear         = 0.0f;
    l.quadratic      = 1.0f;
    s->AddPointLight(l);
  }

  // LO 1.1: faint ambient 0.03*albedo (no IBL yet), Reinhard tone + gamma,
  // raw 0.1 clear (~RGB 25). Engine: drive IBL intensity low for the faint
  // ambient, use the new LO Reinhard tone, and feed a small linear background
  // so the tonemapped result stays dark like LO's raw clear. Engine PBR path;
  // kill bloom/god-rays/TAA/SSAO for the clean LO figure.
  examples::NoSun(*s);
  examples::SolidBackground(*s, glm::vec3(0.0065f, 0.0065f, 0.0065f), 0.03f);
  s->SetReinhardTone(true);
  s->SetBloomEnabled(false);
  s->SetGodRaysStrength(0.0f);
  s->SetTAAEnabled(false);
  s->SetSSAOEnabled(false);
  s->SetExposure(1.0f);
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildPbrLighting, "LO 6.1.1 PBR lighting", {0, 0.0f, 0},
                                           0.0f, 8.0f, 21.0f, 45.0f});
}
