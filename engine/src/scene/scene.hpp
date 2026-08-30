/**
 * @file scene.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-16
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <vector>

#include "core/logger.hpp"
#include "render/light.hpp"
#include "scene/camera.hpp"
#include "scene/entity.hpp"

namespace MEngine {

class Renderer;

class Scene {
 public:
  Scene();
  ~Scene();

  Entity CreateEntity(const std::string &name = "Unnamed Entity") {
    Entity entity = Entity(registry_.create(), &registry_);
    entity.AddComponent<Tag>(name);
    entities_.push_back(entity);
    return entity;
  }

  void DestroyEntity(Entity entity) {
    // TODO
    registry_.destroy(entity.GetHandle());

    for (auto it = entities_.begin(); it != entities_.end(); ++it) {
      if (*it == entity) {
        entities_.erase(it);
        break;
      }
    }
  }

  template <typename... Components>
  auto GetAllEntitiesWith() {
    auto                view = registry_.view<Components...>();
    std::vector<Entity> entities;
    for (auto entity : view) {
      entities.push_back(Entity(entity, &registry_));
    }
    return entities;
  }

  std::vector<Entity> &GetAllEntities() { return entities_; }

  void LoadScene(const std::string &path);
  void SaveScene(const std::string &path);

  Ref<Camera2D> GetDefaultCameraInfo() { return default_camera_info_; }

  void OnUpdateEditor(Camera2D &camera);

  void OnUpdateSimulation(float dt, Camera2D &camera);

  void OnUpdateRuntime(float dt, int vw, int vh);

  void Render(Camera2D &camera);

  /// @brief Draw all entities with a MeshComponent using the given camera.
  void RenderMeshes(const glm::mat4 &proj_view, const glm::vec3 &camera_pos);

  void AddPointLight(const PointLight &light);
  void ClearPointLights();

  [[nodiscard]] const DirectionalLight &GetLight() const;
  DirectionalLight &GetLight();
  void SetLight(const DirectionalLight &light);

  void SetExposure(float exposure);
  void SetBloomStrength(float strength);
  void SetBloomThreshold(float threshold);

 private:
  entt::registry registry_;

  std::vector<Entity> entities_;

  Ref<Camera2D> default_camera_info_;

  Ref<Renderer> renderer_;
};

}  // namespace MEngine