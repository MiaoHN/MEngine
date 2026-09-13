/**
 * @file camera.hpp
 * @brief Unified camera (perspective / orthographic).
 *
 * Replaces the old `Camera2D` / `OrthographicCamera` / `PerspectiveCamera`
 * trio with a single camera. Stores position + euler rotation (degrees) and a
 * projection description.
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace MEngine {

enum class ProjectionType { Perspective, Orthographic };

class Camera {
 public:
  ProjectionType projection_type = ProjectionType::Perspective;
  float          fov_degrees     = 45.0f;   // perspective vertical FOV
  float          ortho_size      = 5.0f;    // orthographic half-height
  float          near_plane      = 0.1f;
  float          far_plane       = 100.0f;
  float          aspect_ratio    = 16.0f / 9.0f;

  glm::vec3 position{0.0f, 0.0f, 3.0f};
  glm::vec3 rotation{0.0f, 0.0f, 0.0f};  // euler degrees: x=pitch, y=yaw, z=roll

  void SetPosition(const glm::vec3 &p) { position = p; }
  void SetRotation(const glm::vec3 &euler) { rotation = euler; }
  void SetAspect(float aspect) { aspect_ratio = aspect; }
  void SetFov(float fov) { fov_degrees = fov; }

  [[nodiscard]] const glm::vec3 &GetPosition() const { return position; }
  [[nodiscard]] const glm::vec3 &GetRotation() const { return rotation; }
  [[nodiscard]] float            GetFov() const { return fov_degrees; }
  [[nodiscard]] float            GetAspect() const { return aspect_ratio; }
  [[nodiscard]] float            GetNear() const { return near_plane; }
  [[nodiscard]] float            GetFar() const { return far_plane; }

  /// @brief Orients the camera to look at a world-space target.
  void LookAt(const glm::vec3 &target);

  /// @brief Forward direction in world space.
  [[nodiscard]] glm::vec3 GetForward() const;

  [[nodiscard]] glm::mat4 GetViewMatrix() const;
  [[nodiscard]] glm::mat4 GetProjectionMatrix() const;
  [[nodiscard]] glm::mat4 GetProjectionView() const { return GetProjectionMatrix() * GetViewMatrix(); }
};

inline void Camera::LookAt(const glm::vec3 &target) {
  const glm::vec3 forward = glm::normalize(target - position);
  const glm::quat q       = glm::quatLookAt(forward, glm::vec3(0.0f, 1.0f, 0.0f));
  rotation                = glm::degrees(glm::eulerAngles(q));
}

inline glm::vec3 Camera::GetForward() const {
  return glm::normalize(glm::mat3(glm::mat4_cast(glm::quat(glm::radians(rotation)))) * glm::vec3(0.0f, 0.0f, -1.0f));
}

inline glm::mat4 Camera::GetViewMatrix() const {
  const glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) *
                              glm::mat4_cast(glm::quat(glm::radians(rotation)));
  return glm::inverse(transform);
}

inline glm::mat4 Camera::GetProjectionMatrix() const {
  if (projection_type == ProjectionType::Orthographic) {
    const float half_width = aspect_ratio * ortho_size;
    return glm::ortho(-half_width, half_width, -ortho_size, ortho_size, near_plane, far_plane);
  }
  return glm::perspective(glm::radians(fov_degrees), aspect_ratio, near_plane, far_plane);
}

}  // namespace MEngine
