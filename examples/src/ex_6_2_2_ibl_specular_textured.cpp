// = LearnOpenGL 6.pbr/2.2.2.ibl_specular_textured (specular IBL, textured)
//   source: LearnOpenGL/src/6.pbr/2.2.2.ibl_specular_textured/ibl_specular_textured.cpp
//
// Port of LO's iconic "PBR materials under IBL" figure on the ENGINE's own
// PBR + IBL path: five textured spheres (rusted_iron / gold / grass / plastic /
// wall) at x = -5..3, y 0, z 2 in the newport_loft HDR environment (set as the
// engine's IBL/skybox env), lit by LO's 4 white HDR point lights + IBL.
//   - albedo maps are decoded sRGB->linear like LO (SetAlbedoSRGB -> pbr_frag
//     u_albedo_srgb); metallic/roughness come from the combined MR maps
//   - engine IBL (irradiance + prefiltered specular; no BRDF LUT - engine uses
//     the simplified prefiltered*F_ibl term) + Reinhard tone + gamma
//   - skybox (the env) is the background; bloom/god-rays/TAA/SSAO off
#include <memory>

#include "core/application.hpp"
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {

/// @brief Textured PBR material (LO layout): albedo (sRGB decoded) + normal +
/// combined MR map (mr.png) + ao, with factors = 1 so the maps fully drive it.
Ref<Material> PbrMat(const std::string &material) {
  const std::string base = "textures/pbr/" + material + "/";
  const auto tex = [&](const char *name) { return AssetManager::Instance().GetTexture(base + name); };
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(examples::PbrShader());
  m->SetAlbedoMap(tex("albedo.png"));
  m->SetAlbedoSRGB(true);  // LO decodes albedo maps sRGB->linear (pow 2.2)
  m->SetNormalMap(tex("normal.png"));
  m->SetMetallicRoughnessMap(tex("mr.png"));  // G=roughness, B=metallic
  m->SetAOMap(tex("ao.png"));
  m->SetMetallicFactor(1.0f);
  m->SetRoughnessFactor(1.0f);
  m->SetSpecularFactor(1.0f);
  return m;
}

std::shared_ptr<Scene> BuildIblSpecularTextured() {
  auto s = std::make_shared<Scene>();

  // --- the five LO materials, exactly LO 2.2.2's layout (x -5..3, y 0, z 2).
  Ref<Mesh> sphere = Mesh::CreateSphere(1.0f, 64);
  const std::pair<std::string, float> mats[5] = {
      {"rusted_iron", -5.0f}, {"gold", -3.0f}, {"grass", -1.0f}, {"plastic", 1.0f}, {"wall", 3.0f}};
  for (const auto &[name, x] : mats) {
    Put(*s, sphere, PbrMat(name), {x, 0.0f, 2.0f}, 1.0f);
  }

  // --- LO's 4 white HDR point lights (color 300, 1/d^2) + IBL.
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

  // No directional sun; the environment provides the ambient (IBL). Keep the
  // skybox ON (the newport_loft env is the background) and IBL at full.
  examples::NoSun(*s);
  s->SetSkyboxEnabled(true);
  s->SetIblIntensity(1.0f);
  s->SetExposure(1.0f);
  s->SetReinhardTone(true);  // LO 2.2.2.pbr.fs: color/(color+1) + gamma
  s->SetBloomEnabled(false);
  s->SetGodRaysStrength(0.0f);
  s->SetTAAEnabled(false);
  s->SetSSAOEnabled(false);
  return s;
}

}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  // Use LO's exact HDR environment (newport_loft) and its vertical-flip-on-load
  // convention so the skybox/background and reflections are upright.
  MEngine::Application::SetEnvironmentHdrPath("textures/hdr/newport_loft.hdr");
  MEngine::Application::SetEnvironmentHdrFlip(true);
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildIblSpecularTextured, "LO 6.2.2 IBL specular_textured",
                                           {0, 0.0f, 2.0f}, 0.0f, 0.0f, 11.0f, 45.0f});
}
