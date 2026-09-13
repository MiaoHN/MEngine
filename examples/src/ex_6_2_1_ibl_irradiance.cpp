// = LearnOpenGL 6.pbr/2.1.2.ibl_irradiance (diffuse-only image-based lighting)
//   source: LearnOpenGL/src/6.pbr/2.1.2.ibl_irradiance/ibl_irradiance.cpp
//
// 1:1 port on the ENGINE's PBR + IBL path: the same 7x7 red metallic/roughness
// matrix and 4 white HDR point lights as the other 6.pbr matrix demos, in the
// newport_loft HDR environment - but with the SPECULAR part of IBL switched OFF
// (engine SetIblSpecular(false)), exactly like LO's 2.1.2.pbr.fs which only
// adds the irradiance-map diffuse term. Metallic spheres therefore look dark
// (no specular IBL yet) except for their direct-light specular highlights - the
// LO "before" step of the specular-IBL figure in 2.2.1.
//   - red albedo 0.5, metallic = row/7, roughness = clamp(col/7, 0.05, 1)
//   - skybox (the env) on, IBL intensity 1, Reinhard tone + gamma (LO pbr.fs
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
std::shared_ptr<Scene> BuildIblIrradiance() {
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

  // newport_loft env as skybox/IBL; no sun. IBL = DIFFUSE ONLY (LO 2.1.2).
  examples::NoSun(*s);
  s->SetSkyboxEnabled(true);
  s->SetIblIntensity(1.0f);
  s->SetIblSpecular(false);  // 2.1.2 has no prefiltered/BRDF specular IBL yet
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
      MEngine::examples::ExampleApp::Setup{BuildIblIrradiance, "LO 6.2.1 IBL irradiance (diffuse)",
                                           {0, 0.0f, 0}, 0.0f, 8.0f, 21.0f, 45.0f});
}
