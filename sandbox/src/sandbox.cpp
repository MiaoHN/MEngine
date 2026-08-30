#include "sandbox.hpp"

#include "render/model_loader.hpp"
#include "utils/profiler.h"

Sandbox::Sandbox() : Application(GraphicsAPI::OpenGL) {
  active_scene_ = std::make_shared<Scene>();

  lit_shader_ = CreateRef<Shader>("res/shaders/lit_vert.glsl", "res/shaders/lit_frag.glsl");

  // Imported backpack model (OBJ + diffuse texture).
  backpack_mesh_    = ModelLoader::LoadObj("res/models/backpack/backpack.obj");
  backpack_texture_ = Texture::Create("res/models/backpack/diffuse.jpg");

  if (backpack_mesh_) {
    backpack_ = active_scene_->CreateEntity("Backpack");
    auto &transform = backpack_.AddComponent<Transform>();
    // The raw model is roughly centered at (0.05, 0.57, -0.94); translate it
    // so its center lands at the origin for a clean turntable rotation.
    transform.translation = glm::vec3(0.05f, -0.57f, 0.94f);
    backpack_.AddComponent<MeshComponent>(backpack_mesh_, lit_shader_, backpack_texture_);
  }

  camera_.SetPosition(glm::vec3(0.0f, 0.0f, 7.0f));
  camera_.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));
  camera_.SetAspect(16.0f / 9.0f);
}

Sandbox::~Sandbox() {}

void Sandbox::Initialize() {}

void Sandbox::OnUpdate(float dt) {
  PROFILER_FUNCTION();

  if (backpack_.HasComponent<Transform>()) {
    auto &transform = backpack_.GetComponent<Transform>();
    transform.rotation.y += rotation_speed_ * dt;
  }

  active_scene_->RenderMeshes(camera_.GetProjectionView(), camera_.GetPosition());
}

Application *CreateApplication() { return new Sandbox(); }
