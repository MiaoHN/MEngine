// User model (Cerberus by Andrew Maximov) shown in the LO 6.pbr 2.2.2 scene
// environment (newport_loft HDR IBL): same skybox/env + IBL + LO's 4 white HDR
// point lights as ex_6_2_2, but the glTF model is the hero, floating in the
// room. Camera: right-drag orbit, wheel zoom, WASD/Space/Ctrl fly.
#include <limits>
#include <memory>

#include "core/application.hpp"
#include "example_app.hpp"
#include "example_helpers.hpp"
#include "render/model_loader.hpp"

using namespace MEngine;

namespace {

/// @brief Loads a glTF/GLB mesh + its PBR material (fallback: a plain PBR).
void LoadGltfAsset(const std::string &path, Ref<Mesh> &mesh, Ref<Material> &material) {
  mesh = ModelLoader::LoadGltf(path);
  material = ModelLoader::LoadGltfMaterial(path);
  if (!material) {
    material = examples::Pbr(glm::vec3(0.6f), 0.5f, 0.4f);
  }
  material->SetShader(examples::PbrShader());
}

std::shared_ptr<Scene> BuildIblCerberus() {
  auto s = std::make_shared<Scene>();

  // --- the Cerberus model, auto-centered + normalized, floating in the room.
  Ref<Mesh>     mesh;
  Ref<Material> material;
  LoadGltfAsset("assets/models/Cerberus_by_Andrew_Maximov/Cerberus_LP.glb", mesh, material);

  glm::vec3 bmin(std::numeric_limits<float>::max());
  glm::vec3 bmax(std::numeric_limits<float>::lowest());
  for (const auto &v : mesh->GetVertices()) {
    bmin = glm::min(bmin, v.position);
    bmax = glm::max(bmax, v.position);
  }
  const glm::vec3 center = (bmin + bmax) * 0.5f;
  const float     radius = glm::length(bmax - bmin) * 0.5f;
  const float     scale  = (radius > 1e-6f) ? (1.0f / radius) : 1.0f;

  Entity g = s->CreateEntity("cerberus");
  auto &t  = g.AddComponent<Transform>();
  t.scale       = glm::vec3(scale);
  t.translation = -center * scale + glm::vec3(0.0f, 1.15f, 0.5f);  // hang in the loft
  // Diagonal pose (barrel up-left) like the reference render of the model.
  t.SetRotationAxisAngle(glm::vec3(0.0f, 0.0f, 1.0f), 38.0f);
  g.AddComponent<MeshComponent>(mesh, material);

  // --- LO 2.2.2's 4 white HDR point lights (color 300, 1/d^2) + IBL.
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

  // newport_loft env as skybox/IBL; no sun; Reinhard tone; clean post.
  examples::NoSun(*s);
  s->SetSkyboxEnabled(true);
  s->SetIblIntensity(1.0f);
  s->SetExposure(1.0f);
  s->SetReinhardTone(true);
  s->SetBloomEnabled(false);
  s->SetGodRaysStrength(0.0f);
  s->SetTAAEnabled(false);
  s->SetSSAOEnabled(false);
  return s;
}

}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(1200, 800);
  MEngine::Application::SetEnvironmentHdrPath("textures/hdr/newport_loft.hdr");
  MEngine::Application::SetEnvironmentHdrFlip(true);
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildIblCerberus, "LO 6.2 IBL scene - Cerberus",
                                           {0, 1.1f, 0.5f}, 0.0f, 2.0f, 3.0f, 45.0f});
}
