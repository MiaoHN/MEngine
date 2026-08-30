#include "sandbox.hpp"

#include <cmath>
#include <limits>

#include "render/model_loader.hpp"
#include "utils/profiler.h"

Sandbox::Sandbox() : Application(GraphicsAPI::OpenGL) {
  active_scene_ = std::make_shared<Scene>();

  pbr_shader_ = CreateRef<Shader>("res/shaders/pbr_vert.glsl", "res/shaders/pbr_frag.glsl");

  // Imported glTF model with a PBR material (metallic-roughness workflow).
  model_mesh_     = ModelLoader::LoadGltf("res/models/damaged_helmet.glb");
  model_material_ = ModelLoader::LoadGltfMaterial("res/models/damaged_helmet.glb");
  if (model_material_) {
    model_material_->SetShader(pbr_shader_);
  }

  if (model_mesh_ && model_material_) {
    model_ = active_scene_->CreateEntity("DamagedHelmet");
    auto &transform = model_.AddComponent<Transform>();

    // Auto-frame: center the model at the origin and normalize its size, so
    // models authored at arbitrary scales fit the view and the fixed near/far
    // planes.
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

    model_.AddComponent<MeshComponent>(model_mesh_, model_material_);
  }

  // Ground plane to receive the helmet's shadow.
  auto ground_mesh     = Mesh::CreatePlane(8.0f);
  auto ground_material = CreateRef<Material>();
  ground_material->SetShader(pbr_shader_);
  ground_material->SetBaseColorFactor(glm::vec4(0.5f, 0.5f, 0.55f, 1.0f));
  ground_material->SetRoughnessFactor(0.9f);
  ground_material->SetMetallicFactor(0.0f);

  auto ground_entity = active_scene_->CreateEntity("Ground");
  auto &ground_transform = ground_entity.AddComponent<Transform>();
  ground_transform.translation = glm::vec3(0.0f, -1.3f, 0.0f);
  ground_entity.AddComponent<MeshComponent>(ground_mesh, ground_material);

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
