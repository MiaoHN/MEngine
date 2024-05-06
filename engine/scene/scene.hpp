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

  Entity CreateEntity() {
    Entity entity = Entity(registry_.create(), &registry_);
    entities_.push_back(entity);
    return entity;
  }

  void DestroyEntity(Entity entity) {
    // TODO
    // registry_.destroy(entity.GetHandle());
    // std::remove(entities_.begin(), entities_.end(), entity);
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

  std::shared_ptr<OrthographicCamera> GetCamera() { return camera_; }

  void LoadScene(const std::string& path);
  void SaveScene(const std::string& path);

 private:
  entt::registry registry_;

  std::vector<Entity> entities_;

  std::shared_ptr<OrthographicCamera> camera_;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine