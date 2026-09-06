// LearnOpenGL "Lighting Maps" (diffuse) - container2 albedo texture on cubes,
// lit by a sun + a close white point light so the polished surface shows a
// bright specular highlight (PBR: low roughness ~ specular map).
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildLightingMaps() {
  auto s = std::make_shared<Scene>();

  Put(*s, Mesh::CreatePlane(12.0f), examples::Pbr(glm::vec3(0.16f, 0.16f, 0.19f), 0.0f, 0.9f), {0, 0, 0});

  // Wooden-crate material: container2 albedo, fairly smooth so we see the
  // texture's own highlights under the point light.
  const auto crate = [&]() { return examples::PbrTextured("textures/container2.png", "", 0.32f, 0.0f); };
  Put(*s, Mesh::CreateCube(), crate(), {0.0f, 1.05f, 0.0f}, 2.0f);
  Put(*s, Mesh::CreateCube(), crate(), {3.0f, 0.55f, -1.2f}, 1.0f);
  Put(*s, Mesh::CreateCube(), crate(), {-2.8f, 0.45f, 0.9f}, 0.8f);

  examples::Sun(*s, {-0.4f, -1.0f, -0.25f}, glm::vec3(1.1f, 1.0f, 0.9f));
  examples::SolidBackground(*s, glm::vec3(0.10f, 0.10f, 0.12f), 0.10f);
  s->SetTAAEnabled(true);

  PointLight l;
  l.position  = {2.2f, 3.0f, 2.4f};
  l.color     = glm::vec3(1.0f, 0.98f, 0.92f);
  l.intensity = 34.0f;
  l.radius    = 14.0f;
  s->AddPointLight(l);

  PointLight l2;
  l2.position  = {-2.4f, 1.4f, -2.0f};
  l2.color     = glm::vec3(0.5f, 0.6f, 1.0f);
  l2.intensity = 12.0f;
  l2.radius    = 10.0f;
  s->AddPointLight(l2);
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildLightingMaps, "Lighting Maps (textures)", {0, 1.0f, 0}, 24.0f, 16.0f, 9.5f});
}
