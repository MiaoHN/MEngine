// LearnOpenGL "Colors" - object color * light color, nothing else.
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildColors() {
  auto s = std::make_shared<Scene>();
  Put(*s, Mesh::CreatePlane(9.0f), examples::Pbr(glm::vec3(0.06f, 0.06f, 0.08f), 0.0f, 0.95f), {0, 0, 0});
  Put(*s, Mesh::CreateCube(), examples::Pbr(glm::vec3(1.0f, 0.35f, 0.25f), 0.0f, 0.6f), {0, 1.0f, 0}, 1.2f);

  examples::Sun(*s, {-0.3f, -1.0f, -0.4f}, glm::vec3(1.0f, 0.95f, 0.9f));
  examples::SolidBackground(*s, glm::vec3(0.10f, 0.10f, 0.12f), 0.05f);
  s->SetExposure(1.0f);
  s->SetTAAEnabled(true);
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildColors, "Colors", {0, 0.8f, 0}, 0.0f, 12.0f, 6.5f});
}
