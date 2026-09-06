// LearnOpenGL "HDR + Bloom" - bright bulbs (very intense point lights at
// polished white cubes) over a dark floor, with the bloom pass enabled.
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildHdrBloom() {
  auto s = std::make_shared<Scene>();
  Put(*s, Mesh::CreatePlane(30.0f), examples::Pbr(glm::vec3(0.05f, 0.05f, 0.07f), 0.0f, 0.9f), {0, 0, 0});

  const glm::vec3 bulb_col[4] = {{1.0f, 0.25f, 0.2f}, {0.3f, 1.0f, 0.35f}, {0.3f, 0.6f, 1.0f}, {1.0f, 0.9f, 0.3f}};
  for (int i = 0; i < 4; ++i) {
    const float x = static_cast<float>(i - 1) * 2.6f;
    Put(*s, Mesh::CreateCube(), examples::Pbr(glm::vec3(1.0f), 0.0f, 0.04f), {x, 1.4f, 0.0f}, 0.45f);
    PointLight l;
    l.position  = {x, 1.4f, 0.0f};
    l.color     = bulb_col[i];
    l.intensity = 70.0f;
    l.radius    = 14.0f;
    s->AddPointLight(l);
    Put(*s, Mesh::CreateCube(), examples::Pbr(bulb_col[i] * 0.25f, 0.0f, 0.06f), {x * 0.5f, 0.5f, 1.6f}, 0.9f);
  }

  examples::Sun(*s, {-0.3f, -1.0f, -0.4f}, glm::vec3(0.18f, 0.18f, 0.2f));
  examples::SolidBackground(*s, glm::vec3(0.03f, 0.03f, 0.04f), 0.06f);
  s->SetExposure(0.85f);
  s->SetBloomEnabled(true);
  s->SetBloomThreshold(0.8f);
  s->SetBloomStrength(0.45f);
  s->SetTAAEnabled(true);
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildHdrBloom, "HDR + Bloom", {0, 1.6f, 0}, 0.0f, 13.0f, 9.5f});
}
