#include "sandbox.hpp"
#include "utils/profiler.h"

Sandbox::Sandbox() : Application(GraphicsAPI::OpenGL) {
  active_scene_ = std::make_shared<Scene>();

  cube_mesh_  = Mesh::CreateCube(1.0f);
  lit_shader_ = CreateRef<Shader>("res/shaders/lit_vert.glsl", "res/shaders/lit_frag.glsl");

  rotating_cube_ = active_scene_->CreateEntity("Rotating Cube");
  rotating_cube_.AddComponent<Transform>();
  rotating_cube_.AddComponent<MeshComponent>(cube_mesh_, lit_shader_);

  camera_.SetPosition(glm::vec3(0.0f, 0.5f, 3.0f));
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

  active_scene_->RenderMeshes(camera_.GetProjectionView(), camera_.GetPosition());
}

Application *CreateApplication() { return new Sandbox(); }
