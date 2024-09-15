#include "scene/scene.hpp"

#include "render/gl.hpp"
#include "render/renderer.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"
#include "scene/component.hpp"

namespace MEngine {

Scene::Scene() {
  logger_              = Logger::Get("Scene");
  default_camera_info_ = std::make_shared<Camera2D>(-1.6f, 1.6f, -0.9f, 0.9f, 1.0f, true);

  renderer_ = std::make_shared<Renderer>();
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

}  // namespace MEngine
