// = LearnOpenGL 6.pbr/2.2.1.ibl_specular (specular image-based lighting,
//   untextured)  source: LearnOpenGL/src/6.pbr/2.2.1.ibl_specular/ibl_specular.cpp
//
// 1:1 port on the ENGINE's PBR + IBL path: the iconic 7x7 red metallic/roughness
// matrix under the newport_loft HDR environment, lit by LO's 4 white HDR point
// lights + full IBL. This is the "after" step of 2.1.2: the prefiltered specular
// IBL + split-sum BRDF LUT are ON, so the metallic spheres now show sharp
// environment reflections. ex_6_2_1_ibl_irradiance is the same scene with the
// specular IBL off (LO 2.1.2) - run the two side by side to see the step.
//   - red albedo 0.5, metallic = row/7, roughness = clamp(col/7, 0.05, 1)
//   - engine split-sum BRDF LUT specular IBL, Reinhard tone + gamma (LO pbr.fs
//     and background.fs both do color/(color+1) + gamma)
//   - bloom/god-rays/TAA/SSAO off
#include <algorithm>
#include <memory>

#include "core/application.hpp"
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildIblSpecular() {
  auto s = std::make_shared<Scene>();

  // --- the 7x7 PBR sphere matrix (LO layout: (col-3, row-3) * 2.5).
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

  // --- LO's 4 white HDR point lights (color 300, 1/d^2).
  const glm::vec3 lp[4] = {{-10.0f, 10.0f, 10.0f}, {10.0f, 10.0f, 10.0f},
                           {-10.0f, -10.0f, 10.0f}, {10.0f, -10.0f, 10.0f}};
  for (const glm::vec3 &p : lp) {
    PointLight l;
    l.position       = p;
    l.color          = glm::vec3(300.0f);
    l.ambient        = glm::vec3(0.0f);
    l.lo_attenuation = true;
    l.constant       = 0.0f;  // 1 / d^2
    l.linear         = 0.0f;
    l.quadratic      = 1.0f;
    s->AddPointLight(l);
  }

  // newport_loft env as skybox/IBL; no sun. Full IBL (diffuse + specular).
  examples::NoSun(*s);
  s->SetSkyboxEnabled(true);
  s->SetIblIntensity(1.0f);
  s->SetIblSpecular(true);  // 2.2.1 adds the split-sum specular IBL
  s->SetExposure(1.0f);
  s->SetReinhardTone(true);  // LO pbr.fs: color/(color+1) + gamma
  s->SetBloomEnabled(false);
  s->SetGodRaysStrength(0.0f);
  s->SetTAAEnabled(false);
  s->SetSSAOEnabled(false);
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);
  MEngine::Application::SetEnvironmentHdrPath("textures/hdr/newport_loft.hdr");
  MEngine::Application::SetEnvironmentHdrFlip(true);
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildIblSpecular, "LO 6.2.2 IBL specular (matrix)",
                                           {0, 0.0f, 0}, 0.0f, 8.0f, 21.0f, 45.0f});
}
