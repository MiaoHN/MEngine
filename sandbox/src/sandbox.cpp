#include "sandbox.hpp"

#include <cmath>
#include <limits>

#include "render/model_loader.hpp"
#include "utils/profiler.h"

Sandbox::Sandbox() : Application(GraphicsAPI::OpenGL) {
  active_scene_ = std::make_shared<Scene>();

  lit_shader_ = CreateRef<Shader>("res/shaders/lit_vert.glsl", "res/shaders/lit_frag.glsl");

  // Imported glTF model (geometry + base-color texture).
  model_mesh_    = ModelLoader::LoadGltf("res/models/duck.glb");
  model_texture_ = ModelLoader::LoadGltfBaseColorTexture("res/models/duck.glb");

  if (model_mesh_) {
    model_ = active_scene_->CreateEntity("Duck");
    auto &transform = model_.AddComponent<Transform>();

    // Auto-frame: center the model at the origin and normalize its size, so
    // models authored at arbitrary scales (e.g. duck.glb is ~165 units wide)
    // still fit the view and the fixed near/far planes.
    glm::vec3 min(std::numeric_limits<float>::max());
    glm::vec3 max(std::numeric_limits<float>::lowest());
    for (const auto &vertex : model_mesh_->GetVertices()) {
      min = glm::min(min, vertex.position);
      max = glm::max(max, vertex.position);
    }
    const glm::vec3 center = (min + max) * 0.5f;
    const float     radius = glm::length(max - min) * 0.5f;
    const float     scale  = (radius > 1e-6f) ? (1.0f / radius) : 1.0f;

    transform.scale       = glm::vec3(scale);
    transform.translation = -center * scale;

    const float half_fov = glm::radians(camera_.GetFov()) * 0.5f;
    const float distance = (1.0f / std::tan(half_fov)) * 1.2f + 1.0f;
    camera_.SetPosition(glm::vec3(0.0f, 0.0f, distance));
    camera_.LookAt(glm::vec3(0.0f, 0.0f, 0.0f));

    model_.AddComponent<MeshComponent>(model_mesh_, lit_shader_, model_texture_);
  }

  camera_.SetAspect(16.0f / 9.0f);
}

Sandbox::~Sandbox() {}

void Sandbox::Initialize() {}

void Sandbox::OnUpdate(float dt) {
  PROFILER_FUNCTION();

  if (model_.HasComponent<Transform>()) {
    auto &transform = model_.GetComponent<Transform>();
    transform.rotation.y += rotation_speed_ * dt;
  }

  active_scene_->RenderMeshes(camera_.GetProjectionView(), camera_.GetPosition());
}

Application *CreateApplication() { return new Sandbox(); }
