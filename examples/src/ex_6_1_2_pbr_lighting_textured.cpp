// = LearnOpenGL 6.pbr/1.2.lighting_textured (PBR direct lighting, textured)
//   source: LearnOpenGL/src/6.pbr/1.2.lighting_textured/lighting_textured.cpp
//
// 1:1-ish port of LO's textured PBR demo on the ENGINE's PBR path: the 7x7
// rusted_iron sphere matrix (LO loads albedo/normal/metallic/roughness/ao for
// the whole grid) lit by one white HDR point light at (0,0,10), intensity 150,
// LO 1/d^2 attenuation.
//   - engine loads the rusted_iron albedo + normal + ao maps raw (LO also
//     loads them without sRGB decode) and uses scalar metallic/roughness
//     factors: the engine packs roughness+metallic into ONE combined MR map
//     (G=roughness, B=metallic), while LO ships them as two separate grayscale
//     maps - so per-pixel metal/rough are approximated by uniform factors here
//   - engine PBR, IBL off, skybox off, LO clear 0.1, ACES tone + gamma
//   - camera pulled back to frame the whole matrix, 800x600 4:3
#include <memory>

#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {

/// @brief rusted_iron PBR material: raw albedo/normal/ao maps + factors.
Ref<Material> RustedIron() {
  const auto tex = [](const char *name) {
    return AssetManager::Instance().GetTexture(std::string("textures/pbr/rusted_iron/") + name);
  };
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(examples::PbrShader());
  m->SetAlbedoMap(tex("albedo.png"));
  m->SetNormalMap(tex("normal.png"));
  m->SetAOMap(tex("ao.png"));
  m->SetMetallicFactor(0.9f);    // rusted iron: mostly metal
  m->SetRoughnessFactor(0.6f);   // (engine has no separate roughness map)
  m->SetSpecularFactor(1.0f);
  return m;
}

std::shared_ptr<Scene> BuildPbrTextured() {
  auto s = std::make_shared<Scene>();

  // --- the 7x7 rusted_iron sphere matrix (LO layout, radius 1, spacing 2.5).
  Ref<Mesh>     sphere = Mesh::CreateSphere(1.0f, 64);
  Ref<Material> rust   = RustedIron();
  const int rows    = 7;
  const int columns = 7;
  const float spacing = 2.5f;
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < columns; ++col) {
      const glm::vec3 pos(static_cast<float>(col - columns / 2) * spacing,
                          static_cast<float>(row - rows / 2) * spacing, 0.0f);
      Put(*s, sphere, rust, pos, 1.0f);
    }
  }

  // --- one white HDR point light, LO 1.2 (color 150 at (0,0,10), 1/d^2).
  PointLight l;
  l.position       = {0.0f, 0.0f, 10.0f};
  l.color          = glm::vec3(150.0f);
  l.ambient        = glm::vec3(0.0f);
  l.diffuse        = glm::vec3(150.0f);
  l.specular       = glm::vec3(150.0f);
  l.lo_attenuation = true;
  l.constant       = 0.0f;  // 1 / d^2
  l.linear         = 0.0f;
  l.quadratic      = 1.0f;
  s->AddPointLight(l);

  // No IBL / no sun; LO's raw 0.1 clear reads ~RGB 25 while the engine's post
  // tone+gamma would lift 0.1 to a medium grey, so feed ~0.011 linear to keep
  // the background dark like LO. Engine PBR post (ACES tone + gamma), clean LO
  // figure (no bloom / god rays / TAA / SSAO).
  examples::NoSun(*s);
  examples::SolidBackground(*s, glm::vec3(0.011f, 0.011f, 0.011f), 0.0f);
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
      MEngine::examples::ExampleApp::Setup{BuildPbrTextured, "LO 6.1.2 PBR lighting_textured", {0, 0.0f, 0},
                                           0.0f, 8.0f, 21.0f, 45.0f});
}
