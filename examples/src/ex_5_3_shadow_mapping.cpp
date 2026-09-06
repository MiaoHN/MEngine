// LearnOpenGL "Shadow Mapping" - directional shadows (floor + wall + boxes)
// plus a shadow-casting point light with spheres.
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildShadowMapping() {
  auto s = std::make_shared<Scene>();
  Put(*s, Mesh::CreatePlane(40.0f), examples::Pbr(glm::vec3(0.55f, 0.55f, 0.58f), 0.0f, 0.92f), {0, 0, 0});

  // Receiving wall.
  {
    Entity wall = s->CreateEntity("wall");
    auto &tw    = wall.AddComponent<Transform>();
    tw.translation = {-6.0f, 3.0f, -7.0f};
    tw.scale       = {10.0f, 6.0f, 0.8f};
    wall.AddComponent<MeshComponent>(Mesh::CreateCube(), examples::Pbr(glm::vec3(0.8f, 0.8f, 0.85f), 0.0f, 0.8f));
  }

  const glm::vec3 box_pos[4] = {{-2.0f, 0.5f, 1.5f}, {0.5f, 0.5f, -1.5f}, {3.0f, 0.5f, 1.0f}, {0.0f, 1.5f, 0.0f}};
  const glm::vec3 box_col[4] = {{0.9f, 0.3f, 0.3f}, {0.3f, 0.8f, 0.4f}, {0.3f, 0.5f, 1.0f}, {0.9f, 0.8f, 0.3f}};
  for (int i = 0; i < 4; ++i) {
    Put(*s, Mesh::CreateCube(), examples::Pbr(box_col[i], 0.05f, 0.6f), box_pos[i], i == 3 ? 1.2f : 1.0f);
  }

  // Point-light shadow cluster with spheres.
  PointLight pl;
  pl.position     = {4.5f, 2.6f, 4.5f};
  pl.color        = glm::vec3(1.0f, 0.95f, 0.9f);
  pl.intensity    = 34.0f;
  pl.radius       = 12.0f;
  pl.casts_shadow = true;
  s->AddPointLight(pl);
  const glm::vec3 sph_pos[3] = {{2.6f, 0.6f, 2.6f}, {4.5f, 0.6f, 6.6f}, {6.2f, 0.6f, 3.2f}};
  for (int i = 0; i < 3; ++i) {
    Put(*s, Mesh::CreateSphere(0.6f, 24), examples::Pbr(glm::vec3(0.9f, 0.85f, 0.8f), 0.3f, 0.25f), sph_pos[i]);
  }

  examples::Sun(*s, {-0.5f, -1.0f, -0.25f}, glm::vec3(1.25f, 1.2f, 1.1f));
  examples::SolidBackground(*s, glm::vec3(0.11f, 0.11f, 0.13f), 0.20f);
  s->SetExposure(1.0f);
  s->SetTAAEnabled(true);
  s->SetShadowPcfRadius(4.0f);
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildShadowMapping, "Shadow Mapping", {0, 1.5f, 0}, 35.0f, 24.0f, 16.0f});
}
