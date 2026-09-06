// = LearnOpenGL 3.model_loading/1.model_loading
//   source: LearnOpenGL/src/3.model_loading/1.model_loading/model_loading.cpp
//
// 1:1 port: the backpack model loaded from OBJ at the origin, shown exactly as
// LO 3.1 draws it - the 1.model_loading.fs just samples the diffuse texture
// (no lighting), so the material is engine-PBR set to UNLIT with the raw
// diffuse map, and the composite is set to linear output (no tone/gamma) so
// the sRGB texture bytes go straight to the screen like LO's unlit draw.
//   - clear 0.05 grey (LO), no lights/skybox
//   - camera (0,0,3) FOV 45 (LO default), 800x600
#include <limits>
#include <memory>

#include "core/application.hpp"
#include "example_app.hpp"
#include "example_helpers.hpp"
#include "render/model_loader.hpp"

using namespace MEngine;

namespace {
std::shared_ptr<Scene> BuildModelLoading() {
  auto s = std::make_shared<Scene>();

  Ref<Mesh> mesh = ModelLoader::LoadObj("assets/models/backpack/backpack.obj");
  Ref<Material> material = CreateRef<Material>();
  material->SetShader(examples::PbrShader());
  material->SetAlbedoMap(AssetManager::Instance().GetTexture("models/backpack/diffuse.jpg"));
  material->SetUnlit(true);  // LO 3.1.fs: FragColor = texture(diffuse)

  // Center + normalize so the backpack fills the frame from the shared orbit
  // camera (its OBJ is authored much larger than a unit scene).
  glm::vec3 bmin(std::numeric_limits<float>::max());
  glm::vec3 bmax(std::numeric_limits<float>::lowest());
  for (const auto &v : mesh->GetVertices()) {
    bmin = glm::min(bmin, v.position);
    bmax = glm::max(bmax, v.position);
  }
  const glm::vec3 center = (bmin + bmax) * 0.5f;
  const float     radius = glm::length(bmax - bmin) * 0.5f;
  const float     scale  = (radius > 1e-6f) ? (1.0f / radius) : 1.0f;

  Entity e = s->CreateEntity("backpack");
  auto &t  = e.AddComponent<Transform>();
  t.scale       = glm::vec3(scale);
  t.translation = -center * scale;
  e.AddComponent<MeshComponent>(mesh, material);

  // LO 3.1: plain dark-grey clear, no lighting, raw texture to screen.
  examples::NoSun(*s);
  examples::LoScene(*s, glm::vec3(0.05f, 0.05f, 0.05f));
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildModelLoading, "LO 3.1 model_loading (backpack)",
                                           {0, 0.0f, 0}, 0.0f, 0.0f, 3.0f, 45.0f});
}
