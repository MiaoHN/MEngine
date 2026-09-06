// LearnOpenGL "Normal Mapping" - a tiled brick wall whose surface is perturbed
// by brickwall_normal.jpg (the engine's PBR normal-map slot). A low grazing
// light from the side makes the brick relief read clearly.
#include "example_app.hpp"
#include "example_helpers.hpp"

using namespace MEngine;
using MEngine::examples::Put;

namespace {
std::shared_ptr<Scene> BuildNormalMapping() {
  auto s = std::make_shared<Scene>();

  Put(*s, Mesh::CreatePlane(10.0f), examples::Pbr(glm::vec3(0.28f, 0.28f, 0.32f), 0.0f, 0.85f), {0, 0, -2.0f});

  // The normal-mapped brick wall, standing at z=0 facing the camera (+Z).
  Ref<Mesh> wall_mesh = examples::TiledWall(7.0f, 5.0f, 4.0f);
  Ref<Material> wall_mat = examples::PbrTextured("textures/brickwall.jpg", "textures/brickwall_normal.jpg", 0.85f);
  {
    Entity wall = s->CreateEntity("brick_wall");
    auto &t     = wall.AddComponent<Transform>();
    t.translation = {0.0f, 2.5f, 0.0f};
    t.scale       = glm::vec3(1.0f);
    wall.AddComponent<MeshComponent>(wall_mesh, wall_mat);
  }

  // A couple of plain cubes for scale/shadow reference.
  Put(*s, Mesh::CreateCube(), examples::Pbr(glm::vec3(0.55f, 0.55f, 0.6f), 0.0f, 0.5f), {2.6f, 0.5f, -0.8f});
  Put(*s, Mesh::CreateCube(), examples::Pbr(glm::vec3(0.9f, 0.4f, 0.3f), 0.0f, 0.5f), {-2.8f, 0.5f, -0.8f});

  // Low grazing light from the side shows the normal-map relief best.
  examples::Sun(*s, {-0.75f, -0.35f, -0.55f}, glm::vec3(1.25f, 1.15f, 1.0f));
  examples::SolidBackground(*s, glm::vec3(0.10f, 0.10f, 0.12f), 0.08f);
  s->SetTAAEnabled(true);

  PointLight l;
  l.position  = {3.2f, 3.2f, 2.2f};
  l.color     = glm::vec3(1.0f, 0.95f, 0.9f);
  l.intensity = 30.0f;
  l.radius    = 12.0f;
  s->AddPointLight(l);
  return s;
}
}  // namespace

::MEngine::Application *CreateApplication() {
  MEngine::Application::SetStartupWindowSize(800, 600);  // LO's 800x600 (4:3)
  return new MEngine::examples::ExampleApp(
      MEngine::examples::ExampleApp::Setup{BuildNormalMapping, "Normal Mapping", {0, 2.5f, 0.0f}, 0.0f, 4.0f, 9.0f});
}
