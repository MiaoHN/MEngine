#include "scene/scene.hpp"

#include "render/renderer.hpp"
#include "scene/component.hpp"

namespace MEngine {

Scene::Scene() {
  default_camera_info_                = CreateRef<Camera>();
  default_camera_info_->projection_type = ProjectionType::Orthographic;
  default_camera_info_->ortho_size      = 5.0f;
  default_camera_info_->near_plane      = -100.0f;
  default_camera_info_->far_plane       = 100.0f;
  default_camera_info_->position        = glm::vec3(0.0f);

  renderer_ = CreateRef<Renderer>();
}

Scene::~Scene() {}

void Scene::LoadScene(const std::string &path) {
  (void)path;
  LOG_WARN("Scene") << "LoadScene is not implemented yet: " << path;
}

void Scene::SaveScene(const std::string &path) {
  (void)path;
  LOG_WARN("Scene") << "SaveScene is not implemented yet: " << path;
}

void Scene::OnUpdateEditor(const Camera &camera) { Render(camera); }

void Scene::OnUpdateSimulation(float dt, const Camera &camera) {
  (void)dt;
  // TODO: Update scene status
  Render(camera);
}

void Scene::OnUpdateRuntime(float dt, int vw, int vh) {
  (void)dt;
  // TODO: Implement
  bool has_primary_camera = false;
  for (auto &entity : GetAllEntitiesWith<CameraComponent>()) {
    auto &component = entity.GetComponent<CameraComponent>();
    if (component.primary) {
      has_primary_camera = true;
      component.camera.SetAspect(static_cast<float>(vw) / static_cast<float>(vh));
      Render(component.camera);
    }
  }
  if (!has_primary_camera) {
    Render(*GetDefaultCameraInfo());
  }
}

void Scene::Render(const Camera &camera) {
  const glm::mat4 proj_view = camera.GetProjectionView();
  for (auto &entity : GetAllEntitiesWith<Sprite2D>()) {
    auto &sprite = entity.GetComponent<Sprite2D>();
    renderer_->RenderSprite(sprite, proj_view);
  }
  for (auto &entity : GetAllEntitiesWith<AnimatedSprite2D>()) {
    auto &sprite = entity.GetComponent<AnimatedSprite2D>();
    renderer_->RenderSprite(sprite, proj_view);
  }
}

void Scene::RenderFromPrimaryCamera(unsigned int target_fbo, int target_width, int target_height) {
  const float aspect = (target_width > 0 && target_height > 0)
                           ? static_cast<float>(target_width) / static_cast<float>(target_height)
                           : 16.0f / 9.0f;

  const Camera *active = nullptr;
  for (auto &entity : GetAllEntitiesWith<CameraComponent>()) {
    auto &component = entity.GetComponent<CameraComponent>();
    if (component.primary) {
      component.camera.SetAspect(aspect);
      active = &component.camera;
      break;
    }
  }

  if (!active) {
    default_camera_info_->SetAspect(aspect);
    active = default_camera_info_.get();
  }

  RenderMeshes(active->GetViewMatrix(), active->GetProjectionMatrix(), active->GetPosition(), target_fbo, target_width,
               target_height);
}

bool Scene::HasPrimaryCamera() {
  for (auto &entity : GetAllEntitiesWith<CameraComponent>()) {
    if (entity.GetComponent<CameraComponent>().primary) {
      return true;
    }
  }
  return false;
}

void Scene::RenderMeshes(const glm::mat4 &view, const glm::mat4 &proj, const glm::vec3 &camera_pos,
                         unsigned int target_fbo, int target_width, int target_height) {
  auto entities = GetAllEntitiesWith<MeshComponent>();
  if (entities.empty()) {
    return;
  }

  // TAA jitters the camera projection by a sub-pixel each frame; the main pass
  // and skybox use the jittered projection, then the TAA resolve blends it with
  // the history buffer before post-processing.
  const bool       taa_enabled   = renderer_->IsTAAEnabled();
  const glm::mat4  render_proj   = taa_enabled ? renderer_->GetJitteredProjection(proj) : proj;
  const glm::mat4  proj_view     = render_proj * view;

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

  // Omnidirectional (point light) shadow passes: one cube map per
  // shadow-casting point light, rendered face by face.
  {
    const auto &point_lights = renderer_->GetPointLights();
    for (size_t i = 0; i < point_lights.size(); ++i) {
      const int shadow_index = renderer_->GetPointShadowIndex(static_cast<int>(i));
      if (shadow_index < 0) {
        continue;
      }

      const auto transforms = point_lights[i].GetShadowMatrices();
      renderer_->BeginPointShadowPass(shadow_index, point_lights[i].position, point_lights[i].radius);
      for (int face = 0; face < 6; ++face) {
        renderer_->BindPointShadowFace(shadow_index, face, transforms[face]);
        for (auto &entity : entities) {
          auto &component = entity.GetComponent<MeshComponent>();
          if (!component.mesh) {
            continue;
          }
          const glm::mat4 model =
              entity.HasComponent<Transform>() ? entity.GetComponent<Transform>().GetTransform() : glm::mat4(1.0f);
          renderer_->DrawMeshPointShadow(component.mesh, model);
        }
      }
      renderer_->EndPointShadowPass(shadow_index);
    }
  }

  // SSAO geometry pass (view-space position + normal), then the AO passes.
  if (renderer_->IsSSAOEnabled()) {
    renderer_->BeginSSAOPass(proj, view);
    for (auto &entity : entities) {
      auto &component = entity.GetComponent<MeshComponent>();
      if (!component.mesh) {
        continue;
      }
      const glm::mat4 model =
          entity.HasComponent<Transform>() ? entity.GetComponent<Transform>().GetTransform() : glm::mat4(1.0f);
      renderer_->DrawMeshSSAO(component.mesh, model);
    }
    renderer_->EndSSAOPass();
    renderer_->GenerateSSAO(proj, view);
  }

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
  renderer_->RenderSkybox(view, render_proj);
  renderer_->EndScene();

  // TAA resolve blends the jittered frame with the history buffer.
  renderer_->ResolveTAA();

  // Bloom + tone mapping to the target framebuffer.
  renderer_->PostProcess(view, proj, target_fbo, target_width, target_height);
}

void Scene::AddPointLight(const PointLight &light) { renderer_->AddPointLight(light); }

void Scene::ClearPointLights() { renderer_->ClearPointLights(); }

void Scene::AddSpotLight(const SpotLight &light) { renderer_->AddSpotLight(light); }

void Scene::ClearSpotLights() { renderer_->ClearSpotLights(); }

const DirectionalLight &Scene::GetLight() const { return renderer_->GetLight(); }

DirectionalLight &Scene::GetLight() { return renderer_->GetLight(); }

void Scene::SetLight(const DirectionalLight &light) { renderer_->SetLight(light); }

void Scene::SetExposure(float exposure) { renderer_->SetExposure(exposure); }

void Scene::SetBloomStrength(float strength) { renderer_->SetBloomStrength(strength); }

void Scene::SetBloomThreshold(float threshold) { renderer_->SetBloomThreshold(threshold); }

void Scene::SetShadowPcfRadius(float radius) { renderer_->SetShadowPcfRadius(radius); }

void Scene::SetIblIntensity(float intensity) { renderer_->SetIblIntensity(intensity); }

void Scene::SetGodRaysStrength(float strength) { renderer_->SetGodRaysStrength(strength); }

void Scene::SetSSAOEnabled(bool enabled) { renderer_->SetSSAOEnabled(enabled); }

void Scene::SetTAAEnabled(bool enabled) { renderer_->SetTAAEnabled(enabled); }

void Scene::SetBloomEnabled(bool enabled) { renderer_->SetBloomEnabled(enabled); }

bool Scene::IsSSAOEnabled() const { return renderer_->IsSSAOEnabled(); }

bool Scene::IsTAAEnabled() const { return renderer_->IsTAAEnabled(); }

bool Scene::IsBloomEnabled() const { return renderer_->IsBloomEnabled(); }

float Scene::GetExposure() const { return renderer_->GetExposure(); }

float Scene::GetBloomStrength() const { return renderer_->GetBloomStrength(); }

float Scene::GetBloomThreshold() const { return renderer_->GetBloomThreshold(); }

float Scene::GetShadowPcfRadius() const { return renderer_->GetShadowPcfRadius(); }

float Scene::GetIblIntensity() const { return renderer_->GetIblIntensity(); }

float Scene::GetGodRaysStrength() const { return renderer_->GetGodRaysStrength(); }

void Scene::SetRenderMode(RenderMode mode) { renderer_->SetRenderMode(mode); }

RenderMode Scene::GetRenderMode() const { return renderer_->GetRenderMode(); }

}  // namespace MEngine
