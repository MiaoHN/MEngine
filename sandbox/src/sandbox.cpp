#include "sandbox.hpp"

#include "render/model_loader.hpp"
#include "utils/profiler.h"

Sandbox::Sandbox() : Application(GraphicsAPI::OpenGL) {
  active_scene_ = std::make_shared<Scene>();

  lit_shader_ = CreateRef<Shader>("res/shaders/lit_vert.glsl", "res/shaders/lit_frag.glsl");

  // Procedural cube on the left.
  cube_mesh_ = Mesh::CreateCube(1.0f);
  rotating_cube_ = active_scene_->CreateEntity("Cube");
  auto &cube_transform = rotating_cube_.AddComponent<Transform>();
  cube_transform.translation = glm::vec3(-1.5f, 0.0f, 0.0f);
  rotating_cube_.AddComponent<MeshComponent>(cube_mesh_, lit_shader_);

  // Imported OBJ sphere on the right.
  sphere_mesh_ = ModelLoader::LoadObj("res/models/sphere.obj");
  if (sphere_mesh_) {
    rotating_sphere_ = active_scene_->CreateEntity("Sphere");
    auto &sphere_transform = rotating_sphere_.AddComponent<Transform>();
    sphere_transform.translation = glm::vec3(1.5f, 0.0f, 0.0f);
    rotating_sphere_.AddComponent<MeshComponent>(sphere_mesh_, lit_shader_);
  }

  camera_.SetPosition(glm::vec3(0.0f, 0.0f, 4.0f));
  camera_.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));
  camera_.SetAspect(16.0f / 9.0f);
}

Sandbox::~Sandbox() {}

void Sandbox::Initialize() {}

void Sandbox::OnUpdate(float dt) {
  PROFILER_FUNCTION();

  if (rotating_cube_.HasComponent<Transform>()) {
    auto &transform = rotating_cube_.GetComponent<Transform>();
    transform.rotation.y += rotation_speed_ * dt;
    transform.rotation.x += rotation_speed_ * 0.5f * dt;
  }

  if (rotating_sphere_.HasComponent<Transform>()) {
    auto &transform = rotating_sphere_.GetComponent<Transform>();
    transform.rotation.y -= rotation_speed_ * dt;
  }

  active_scene_->RenderMeshes(camera_.GetProjectionView(), camera_.GetPosition());
}

Application *CreateApplication() { return new Sandbox(); }
