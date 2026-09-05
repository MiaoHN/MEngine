#include "sandbox.hpp"

#include <cmath>
#include <limits>

#include "render/asset_manager.hpp"
#include "render/model_loader.hpp"
#include "utils/profiler.h"

Sandbox::Sandbox() : Application(GraphicsAPI::OpenGL) {
  active_scene_ = std::make_shared<Scene>();

  // Standalone play: load a scene saved by the editor instead of the demo.
  const std::string &scene_path = Application::GetStartupScenePath();
  if (!scene_path.empty()) {
    active_scene_->LoadScene(scene_path);
    active_scene_->StartSimulation();
    running_loaded_scene_ = true;
    return;
  }

  pbr_shader_ = AssetManager::Instance().GetShader("pbr");

  // Imported glTF model with a PBR material (metallic-roughness workflow).
  const std::string helmet_path = AssetManager::Instance().Resolve("models/damaged_helmet.glb");
  model_mesh_                   = ModelLoader::LoadGltf(helmet_path);
  model_material_               = ModelLoader::LoadGltfMaterial(helmet_path);
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

  // Keep the directional light relatively dim so the colored point lights stand out.
  // Point the sun into the view (in front of the camera) so the god rays show.
  active_scene_->GetLight().direction = glm::normalize(glm::vec3(-0.3f, -1.0f, 0.5f));
  active_scene_->GetLight().color     = glm::vec3(0.9f);

  // Strong, saturated warm/cool point lights for clear contrast (both cast
  // omnidirectional cube shadows now).
  PointLight warm;
  warm.position     = glm::vec3(2.5f, 0.5f, 1.2f);
  warm.color        = glm::vec3(1.0f, 0.4f, 0.1f);
  warm.intensity    = 28.0f;
  warm.radius       = 5.0f;
  warm.casts_shadow = true;
  active_scene_->AddPointLight(warm);

  PointLight cool;
  cool.position     = glm::vec3(-2.5f, 0.5f, -1.2f);
  cool.color        = glm::vec3(0.1f, 0.4f, 1.0f);
  cool.intensity    = 28.0f;
  cool.radius       = 5.0f;
  cool.casts_shadow = true;
  active_scene_->AddPointLight(cool);

  // A cool-white spot light aimed at the helmet from above/front.
  SpotLight key;
  key.position  = glm::vec3(0.0f, 3.0f, 2.5f);
  key.direction = glm::normalize(glm::vec3(0.0f, -1.0f, -0.6f));
  key.color     = glm::vec3(1.0f, 1.0f, 1.0f);
  key.intensity = 30.0f;
  key.range     = 12.0f;
  active_scene_->AddSpotLight(key);

  // Post-processing tuning (exposure / bloom strength / bloom threshold).
  active_scene_->SetExposure(1.1f);
  active_scene_->SetBloomStrength(0.015f);
  active_scene_->SetBloomThreshold(1.0f);
  // Wider PCF kernel for visibly softer directional shadows.
  active_scene_->SetShadowPcfRadius(4.0f);
  // The HDR sky is bright; tone the IBL down so metallic surfaces do not
  // wash out while the direct lights keep their intensity.
  active_scene_->SetIblIntensity(0.4f);
  // Screen-space ambient occlusion for contact shadowing.
  active_scene_->SetSSAOEnabled(true);
  // Temporal anti-aliasing.
  active_scene_->SetTAAEnabled(true);
  // Volumetric light (god rays) strength.
  active_scene_->SetGodRaysStrength(0.06f);

  camera_.SetAspect(16.0f / 9.0f);
}

Sandbox::~Sandbox() {}

void Sandbox::Initialize() {}

void Sandbox::OnUpdate(float dt) {
  PROFILER_FUNCTION();

  if (running_loaded_scene_) {
    active_scene_->StepSimulation(dt);
    if (active_scene_->HasPrimaryCamera()) {
      active_scene_->RenderFromPrimaryCamera();
    } else {
      active_scene_->RenderMeshes(camera_.GetViewMatrix(), camera_.GetProjectionMatrix(), camera_.GetPosition());
    }
    return;
  }

  if (model_.HasComponent<Transform>()) {
    auto &transform = model_.GetComponent<Transform>();
    transform.rotation.y += rotation_speed_ * dt;
  }

  active_scene_->RenderMeshes(camera_.GetViewMatrix(), camera_.GetProjectionMatrix(), camera_.GetPosition());
}

Application *CreateApplication() { return new Sandbox(); }
