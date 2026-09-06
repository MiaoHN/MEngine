// LearnOpenGL "Basic Lighting" - diffuse + specular on a simple object.
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildBasicLighting() {
  auto s = std::make_shared<Scene>();
  Put(*s, Mesh::CreatePlane(10.0f), examples::Pbr(glm::vec3(0.16f, 0.16f, 0.2f), 0.0f, 0.9f), {0, 0, 0});
  // A light-grey cube: shows diffuse shading from the sun + a specular flash
  // from the close white point light (low roughness = tight highlight).
  Put(*s, Mesh::CreateCube(), examples::Pbr(glm::vec3(0.75f, 0.75f, 0.8f), 0.0f, 0.25f), {0, 0.8f, 0}, 1.3f);
  Put(*s, Mesh::CreateCube(), examples::Pbr(glm::vec3(0.5f, 0.55f, 0.7f), 0.05f, 0.4f), {2.4f, 0.5f, -1.0f});

  examples::Sun(*s, {-0.4f, -1.0f, -0.3f}, glm::vec3(1.1f, 1.05f, 1.0f));
  examples::SolidBackground(*s, glm::vec3(0.10f, 0.10f, 0.12f), 0.10f);
  s->SetExposure(1.0f);
  s->SetTAAEnabled(true);

  PointLight l;
  l.position  = {2.2f, 2.8f, 2.2f};
  l.color     = glm::vec3(1.0f);
  l.intensity = 22.0f;
  l.radius    = 12.0f;
  s->AddPointLight(l);
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildBasicLighting, "Basic Lighting", {0, 0.9f, 0}, -25.0f, 16.0f, 8.0f});
}
