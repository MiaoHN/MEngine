// LearnOpenGL "Multiple Lights" - a sun + several colored point lights over a
// grid of colored cubes.
#include <cmath>

#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildMultipleLights() {
  auto s = std::make_shared<Scene>();
  Put(*s, Mesh::CreatePlane(24.0f), examples::Pbr(glm::vec3(0.22f, 0.22f, 0.27f), 0.0f, 0.9f), {0, 0, 0});

  const glm::vec3 cube_colors[9] = {
      {1.0f, 0.2f, 0.2f}, {0.6f, 1.0f, 0.2f}, {0.2f, 0.7f, 1.0f},
      {1.0f, 0.6f, 0.1f}, {0.9f, 0.2f, 0.9f}, {0.2f, 0.9f, 0.6f},
      {0.4f, 0.4f, 1.0f}, {1.0f, 0.8f, 0.2f}, {0.6f, 0.9f, 0.3f},
  };
  int i = 0;
  for (int x = -2; x <= 2; x += 2) {
    for (int z = -2; z <= 2; z += 2) {
      Put(*s, Mesh::CreateCube(), examples::Pbr(cube_colors[i++], 0.0f, 0.55f),
          {static_cast<float>(x), 0.5f, static_cast<float>(z)});
    }
  }

  examples::Sun(*s, {-0.35f, -1.0f, -0.5f}, glm::vec3(1.05f, 1.0f, 0.95f));
  examples::SolidBackground(*s, glm::vec3(0.10f, 0.10f, 0.12f), 0.12f);
  s->SetExposure(1.0f);
  s->SetTAAEnabled(true);

  const glm::vec3 colors[5] = {{1.0f, 0.1f, 0.1f}, {0.1f, 1.0f, 0.2f}, {0.1f, 0.4f, 1.0f},
                               {1.0f, 0.8f, 0.1f}, {1.0f, 0.2f, 1.0f}};
  for (int k = 0; k < 5; ++k) {
    const float a = static_cast<float>(k) / 5.0f * 6.2831853f;
    PointLight  l;
    l.position  = {4.6f * std::cos(a), 2.6f, 4.6f * std::sin(a)};
    l.color     = colors[k];
    l.intensity = 9.0f;
    l.radius    = 9.0f;
    s->AddPointLight(l);
  }
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildMultipleLights, "Multiple Lights", {0, 1.0f, 0}, 0.0f, 18.0f, 11.0f});
}
