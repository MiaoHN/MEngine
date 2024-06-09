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
#include "scene/camera.hpp"
#include "scene/entity.hpp"

namespace MEngine {

class Scene {
 public:
  Scene();
  ~Scene();

  Entity CreateEntity(const std::string& name = "Unnamed Entity") {
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

  std::vector<Entity>& GetAllEntities() { return entities_; }

  void LoadScene(const std::string& path);
  void SaveScene(const std::string& path);

  std::shared_ptr<CameraInfo> GetDefaultCameraInfo() {
    return default_camera_info_;
  }

 private:
  entt::registry registry_;

  std::vector<Entity> entities_;

  std::shared_ptr<spdlog::logger> logger_;

  std::shared_ptr<CameraInfo> default_camera_info_;
};

}  // namespace MEngine