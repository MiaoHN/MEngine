// Generic engine model viewer: loads a glTF/GLB asset from assets/models,
// auto-frames it at the origin, and shows it with engine PBR + environment IBL
// + a key light. Camera: right-drag orbit, wheel zoom, WASD/Space/Ctrl fly
// (shared ExampleApp host). Default model = the user-added Cerberus GLB.
//
// NOTE: FBX2glTF's Cerberus GLB only embeds the albedo; the metallic /
// roughness / normal / AO sidecar files are loaded separately (see below).
#include <limits>
#include <memory>

#include "example_app.hpp"
#include "example_helpers.hpp"
#include "render/model_loader.hpp"

using namespace MEngine;

namespace {

std::shared_ptr<Scene> BuildViewer() {
  auto s = std::make_shared<Scene>();

  // Load the model geometry; assemble its PBR material from the original
  // A/M/R/N/AO maps (the GLB itself only carries the albedo).
  const std::string dir   = "assets/models/Cerberus_by_Andrew_Maximov/";
  Ref<Mesh>         mesh  = ModelLoader::LoadGltf(dir + "Cerberus_LP.glb");
  Ref<Material>     material =
      examples::PbrSidecarTextured(dir + "Textures/Cerberus_A.tga", dir + "Textures/Cerberus_N.tga",
                                   dir + "Textures/Cerberus_R.tga", dir + "Textures/Cerberus_M.tga",
                                   dir + "Textures/Raw/Cerberus_AO.tga");
  if (!mesh) {
    return s;  // load failure logged by the loader
  }

  // Auto-frame: center at the origin and normalize to unit size.
  glm::vec3 bmin(std::numeric_limits<float>::max());
  glm::vec3 bmax(std::numeric_limits<float>::lowest());
  for (const auto &v : mesh->GetVertices()) {
    bmin = glm::min(bmin, v.position);
    bmax = glm::max(bmax, v.position);
  }
  const glm::vec3 center = (bmin + bmax) * 0.5f;
  const float     radius = glm::length(bmax - bmin) * 0.5f;
  const float     scale  = (radius > 1e-6f) ? (1.0f / radius) : 1.0f;

  Entity e = s->CreateEntity("model");
  auto &t  = e.AddComponent<Transform>();
  t.scale       = glm::vec3(scale);
  t.translation = -center * scale;
  e.AddComponent<MeshComponent>(mesh, material);

  // IBL environment + a key light so the model reads clearly.
  s->SetSkyboxEnabled(true);
  s->SetIblIntensity(0.8f);
  examples::Sun(*s, {-0.4f, -1.0f, -0.6f}, glm::vec3(1.0f, 0.97f, 0.92f));
  s->SetExposure(1.0f);
  return s;
}

}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(1200, 800);
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildViewer, "Model Viewer (Cerberus)", {0, 0.0f, 0},
                                           0.0f, 8.0f, 2.4f, 45.0f});
}
