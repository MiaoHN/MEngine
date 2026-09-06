// LearnOpenGL "Basic Lighting (Blinn-Phong pipeline)" - the same composition
// as example_basic_lighting but rendered with the classic Blinn-Phong material
// (shader "blinn") so the specular highlight is a hard Blinn lobe like LO.
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildBlinnLighting() {
  auto s = std::make_shared<Scene>();

  Put(*s, Mesh::CreatePlane(10.0f), examples::Blinn(glm::vec3(0.16f, 0.16f, 0.2f), 8.0f, 0.2f), {0, 0, 0});
  // Smooth white-grey cube: strong Blinn highlight off the close point light.
  Put(*s, Mesh::CreateCube(), examples::Blinn(glm::vec3(0.8f, 0.8f, 0.85f), 96.0f, 0.7f), {0, 0.8f, 0}, 1.3f);
  Put(*s, Mesh::CreateCube(), examples::Blinn(glm::vec3(0.6f, 0.4f, 0.3f), 24.0f, 0.4f), {2.4f, 0.5f, -1.0f});

  examples::Sun(*s, {-0.4f, -1.0f, -0.3f}, glm::vec3(1.1f, 1.05f, 1.0f));
  // ibl_intensity doubles as the small Blinn ambient term.
  examples::SolidBackground(*s, glm::vec3(0.10f, 0.10f, 0.12f), 0.18f);
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
      MEngine::examples::ExampleApp::Setup{BuildBlinnLighting, "Blinn-Phong (LO basic lighting)", {0, 0.9f, 0},
                                           -25.0f, 16.0f, 8.0f});
}
