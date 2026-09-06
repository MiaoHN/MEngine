// LearnOpenGL "Materials" - a metallic/roughness sweep on spheres (the PBR
// equivalent of its material-parameter exercise).
#include <cmath>

#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildMaterials() {
  auto s = std::make_shared<Scene>();
  Put(*s, Mesh::CreatePlane(16.0f), examples::Pbr(glm::vec3(0.18f, 0.18f, 0.22f), 0.0f, 0.95f), {0, 0, 0});

  constexpr int kRows = 4;  // metallic
  constexpr int kCols = 8;  // roughness
  const glm::vec3 base(1.0f, 0.55f, 0.25f);  // copper-ish
  for (int r = 0; r < kRows; ++r) {
    for (int c = 0; c < kCols; ++c) {
      const float metallic  = static_cast<float>(r) / (kRows - 1);
      const float roughness = (c == 0) ? 0.05f : 0.05f + static_cast<float>(c) / (kCols - 1) * 0.95f;
      Put(*s, Mesh::CreateSphere(0.42f, 24), examples::Pbr(base, metallic, roughness),
          {static_cast<float>(c - (kCols - 1) / 2) * 1.25f, 0.42f,
           static_cast<float>(r - (kRows - 1) / 2) * 1.25f});
    }
  }

  examples::Sun(*s, {-0.4f, -1.0f, -0.2f}, glm::vec3(1.1f, 1.05f, 0.95f));
  examples::SolidBackground(*s, glm::vec3(0.13f, 0.13f, 0.15f), 0.30f);
  s->SetExposure(1.0f);
  s->SetTAAEnabled(true);

  PointLight l;
  l.position  = {2.2f, 2.6f, 2.2f};
  l.color     = glm::vec3(1.0f);
  l.intensity = 26.0f;
  l.radius    = 14.0f;
  s->AddPointLight(l);
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildMaterials, "Materials", {0, 0.7f, 0}, 0.0f, 16.0f, 9.0f});
}
