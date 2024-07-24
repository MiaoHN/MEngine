/**
 * @file entity.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-21
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <entt/entt.hpp>

namespace MEngine {

class Entity {
 public:
  Entity() = default;
  Entity(entt::entity handle, entt::registry *registry) : handle_(handle), registry_(registry) {}
  ~Entity() = default;

  template <typename T, typename... Args>
  T &AddComponent(Args &&...args) {
    return registry_->emplace<T>(handle_, std::forward<Args>(args)...);
  }

  template <typename T>
  T &GetComponent() {
    return registry_->get<T>(handle_);
  }

  template <typename T>
  bool HasComponent() {
    if (registry_ == nullptr) return false;
    return registry_->all_of<T>(handle_);
  }

  template <typename T>
  void RemoveComponent() {
    registry_->remove<T>(handle_);
  }

  entt::entity GetHandle() const { return handle_; }

  bool operator==(const Entity &other) const { return handle_ == other.handle_ && registry_ == other.registry_; }

  bool operator!=(const Entity &other) const { return !(*this == other); }

 private:
  entt::entity    handle_{entt::null};
  entt::registry *registry_{nullptr};
};

}  // namespace MEngine