#include "scene/scene.hpp"

#include "render/renderer.hpp"
#include "scene/component.hpp"

namespace MEngine {

Scene::Scene() {
  default_camera_info_ = CreateRef<Camera2D>(-1.6f, 1.6f, -0.9f, 0.9f, 1.0f, true);

  renderer_ = CreateRef<Renderer>();
}

Scene::~Scene() {}

void Scene::LoadScene(const std::string &path) {
  // TODO: Implement
}

void Scene::SaveScene(const std::string &path) {
  // TODO: Implement
}

void Scene::OnUpdateEditor(Camera2D &camera) { Render(camera); }

void Scene::OnUpdateSimulation(float dt, Camera2D &camera) {
  // TODO: Update scene status
  Render(camera);
}

void Scene::OnUpdateRuntime(float dt, int vw, int vh) {
  // TODO: Implement
  bool has_primary_camera = false;
  for (auto &entity : GetAllEntitiesWith<Camera2D>()) {
    auto     &camera_info = entity.GetComponent<Camera2D>();
    glm::vec3 position;
    float     rotation;
    if (entity.HasComponent<Sprite2D>()) {
      auto &sprite = entity.GetComponent<Sprite2D>();
      position     = sprite.position;
      rotation     = sprite.rotation.z;
    } else if (entity.HasComponent<AnimatedSprite2D>()) {
      auto &sprite = entity.GetComponent<AnimatedSprite2D>();
      position     = sprite.position;
      rotation     = sprite.rotation.z;
    } else {
      position = glm::vec3(0.0f);
      rotation = 0.0f;
    }
    if (camera_info.primary) {
      has_primary_camera = true;
      camera_info.SetPosition(position);
      camera_info.SetRotation(rotation);
      camera_info.SetProjection(-1.0f, 1.0f, -1.0f, 1.0f);
      camera_info.SetAspectRatio((float)vw / vh);
      Render(camera_info);
    }
  }
  if (!has_primary_camera) {
    Render(*GetDefaultCameraInfo());
  }
}

void Scene::Render(Camera2D &camera) {
  for (auto &entity : GetAllEntitiesWith<Sprite2D>()) {
    auto &sprite = entity.GetComponent<Sprite2D>();
    renderer_->RenderSprite(sprite, camera.GetProjectionView());
  }
  for (auto &entity : GetAllEntitiesWith<AnimatedSprite2D>()) {
    auto &sprite = entity.GetComponent<AnimatedSprite2D>();
    renderer_->RenderSprite(sprite, camera.GetProjectionView());
  }
}

void Scene::RenderMeshes(const glm::mat4 &view, const glm::mat4 &proj, const glm::vec3 &camera_pos) {
  auto entities = GetAllEntitiesWith<MeshComponent>();
  if (entities.empty()) {
    return;
  }

  const glm::mat4 proj_view = proj * view;

  // Directional shadow mapping: models are normalized to a ~1 unit radius by
  // the caller, so a fixed 2-unit orthographic shadow volume covers them.
  const auto      &light          = renderer_->GetLight();
  const glm::mat4 light_view_proj = light.GetLightSpaceMatrix(glm::vec3(0.0f), 2.0f);

  renderer_->BeginShadowPass(light_view_proj);
  for (auto &entity : entities) {
    auto &component = entity.GetComponent<MeshComponent>();
    if (!component.mesh) {
      continue;
    }
    const glm::mat4 model =
        entity.HasComponent<Transform>() ? entity.GetComponent<Transform>().GetTransform() : glm::mat4(1.0f);
    renderer_->DrawMeshShadow(component.mesh, model, light_view_proj);
  }
  renderer_->EndShadowPass();

  // Main pass into the HDR scene framebuffer.
  renderer_->BeginScene();
  for (auto &entity : entities) {
    auto &component = entity.GetComponent<MeshComponent>();
    if (!component.mesh || !component.material) {
      continue;
    }

    const glm::mat4 model =
        entity.HasComponent<Transform>() ? entity.GetComponent<Transform>().GetTransform() : glm::mat4(1.0f);

    renderer_->DrawMesh(component.mesh, component.material, model, proj_view, camera_pos, light_view_proj);
  }

  // Skybox background (drawn after the meshes with depth test LEQUAL).
  renderer_->RenderSkybox(view, proj);
  renderer_->EndScene();

  // Bloom + tone mapping to the default framebuffer.
  renderer_->PostProcess();
}

void Scene::AddPointLight(const PointLight &light) { renderer_->AddPointLight(light); }

void Scene::ClearPointLights() { renderer_->ClearPointLights(); }

const DirectionalLight &Scene::GetLight() const { return renderer_->GetLight(); }

DirectionalLight &Scene::GetLight() { return renderer_->GetLight(); }

void Scene::SetLight(const DirectionalLight &light) { renderer_->SetLight(light); }

void Scene::SetExposure(float exposure) { renderer_->SetExposure(exposure); }

void Scene::SetBloomStrength(float strength) { renderer_->SetBloomStrength(strength); }

void Scene::SetBloomThreshold(float threshold) { renderer_->SetBloomThreshold(threshold); }

}  // namespace MEngine
