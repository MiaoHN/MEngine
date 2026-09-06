// LearnOpenGL "Light Casters" - directional + two spot lights + a fill point
// light over a centre piece.
#include <cmath>

#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildCasters() {
  auto s = std::make_shared<Scene>();
  Put(*s, Mesh::CreatePlane(18.0f), examples::Pbr(glm::vec3(0.35f, 0.33f, 0.3f), 0.0f, 0.85f), {0, 0, 0});
  Put(*s, Mesh::CreateCube(), examples::Pbr(glm::vec3(0.85f, 0.3f, 0.3f), 0.0f, 0.5f), {0.0f, 0.6f, 0.0f}, 1.1f);
  Put(*s, Mesh::CreateSphere(0.5f, 24), examples::Pbr(glm::vec3(0.35f, 0.75f, 0.4f), 0.2f, 0.3f), {0.0f, 2.2f, 0.0f});

  examples::Sun(*s, {-0.4f, -1.0f, -0.3f}, glm::vec3(0.55f, 0.55f, 0.6f));
  examples::SolidBackground(*s, glm::vec3(0.10f, 0.10f, 0.12f), 0.10f);
  s->SetExposure(1.0f);
  s->SetTAAEnabled(true);

  SpotLight warm;
  warm.position     = {3.4f, 6.5f, -3.4f};
  warm.direction    = glm::normalize(glm::vec3(-3.4f, -6.5f, 3.4f));
  warm.color        = glm::vec3(1.0f, 0.95f, 0.85f);
  warm.intensity    = 55.0f;
  warm.range        = 20.0f;
  warm.cutoff       = std::cos(glm::radians(12.0f));
  warm.outer_cutoff = std::cos(glm::radians(19.0f));
  s->AddSpotLight(warm);

  SpotLight cool;
  cool.position     = {-3.4f, 5.5f, 2.5f};
  cool.direction    = glm::normalize(glm::vec3(3.4f, -5.5f, -2.5f));
  cool.color        = glm::vec3(0.45f, 0.6f, 1.0f);
  cool.intensity    = 40.0f;
  cool.range        = 18.0f;
  cool.cutoff       = std::cos(glm::radians(15.0f));
  cool.outer_cutoff = std::cos(glm::radians(22.0f));
  s->AddSpotLight(cool);

  PointLight fill;
  fill.position  = {0.0f, 0.8f, -5.0f};
  fill.color     = glm::vec3(0.3f, 0.3f, 0.35f);
  fill.intensity = 6.0f;
  fill.radius    = 10.0f;
  s->AddPointLight(fill);
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildCasters, "Light Casters", {0, 1.5f, 0}, -45.0f, 16.0f, 11.0f});
}
