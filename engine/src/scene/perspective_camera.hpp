#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MEngine {

/**
 * @brief A perspective camera used for 3D rendering.
 *
 * Simple look-at model: a world-space position, a target point and an up
 * vector. Projection is a standard perspective matrix. This intentionally
 * lives outside the ECS for now, mirroring `OrthographicCamera`; it can be
 * wrapped into a component later when scene cameras are unified.
 */
class PerspectiveCamera {
 public:
  PerspectiveCamera(float fov_degrees = 45.0f, float aspect_ratio = 16.0f / 9.0f, float near_plane = 0.1f,
                    float far_plane = 100.0f)
      : fov_(fov_degrees), aspect_(aspect_ratio), near_(near_plane), far_(far_plane) {}

  void SetPosition(const glm::vec3 &position) { position_ = position; }
  void LookAt(const glm::vec3 &target) { target_ = target; }
  void SetUp(const glm::vec3 &up) { up_ = up; }
  void SetAspect(float aspect_ratio) { aspect_ = aspect_ratio; }
  void SetFov(float fov_degrees) { fov_ = fov_degrees; }

  [[nodiscard]] const glm::vec3 &GetPosition() const { return position_; }
  [[nodiscard]] const glm::vec3 &GetTarget() const { return target_; }
  [[nodiscard]] float GetFov() const { return fov_; }
  [[nodiscard]] float GetAspect() const { return aspect_; }
  [[nodiscard]] float GetNear() const { return near_; }
  [[nodiscard]] float GetFar() const { return far_; }

  [[nodiscard]] glm::mat4 GetViewMatrix() const { return glm::lookAt(position_, target_, up_); }

  [[nodiscard]] glm::mat4 GetProjectionMatrix() const {
    return glm::perspective(glm::radians(fov_), aspect_, near_, far_);
  }

  [[nodiscard]] glm::mat4 GetProjectionView() const { return GetProjectionMatrix() * GetViewMatrix(); }

 private:
  glm::vec3 position_{0.0f, 0.0f, 3.0f};
  glm::vec3 target_{0.0f, 0.0f, 0.0f};
  glm::vec3 up_{0.0f, 1.0f, 0.0f};

  float fov_    = 45.0f;
  float aspect_ = 16.0f / 9.0f;
  float near_   = 0.1f;
  float far_    = 100.0f;
};

}  // namespace MEngine
