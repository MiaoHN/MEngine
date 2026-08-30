/**
 * @file editor_camera.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief Orbit/pan/zoom camera used by the editor viewport.
 *
 * The camera orbits a `target` point. Position is derived from spherical
 * coordinates (yaw / pitch / distance) so that orbiting never drifts the
 * target. Panning moves the target along the camera's right/up axes.
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MEngine {

class EditorCamera {
 public:
  glm::vec3 target{0.0f, 0.0f, 0.0f};

  float yaw      = 45.0f;    // degrees, around +Y
  float pitch    = 30.0f;    // degrees, elevation (positive = above target)
  float distance = 8.0f;

  float fov        = 45.0f;               // vertical FOV, degrees
  float aspect     = 16.0f / 9.0f;        // width / height
  float near_plane = 0.1f;
  float far_plane  = 1000.0f;

  /// @brief Camera eye position derived from the orbit parameters.
  [[nodiscard]] glm::vec3 GetPosition() const {
    const float cos_pitch = std::cos(glm::radians(pitch));
    return target + glm::vec3(distance * cos_pitch * std::sin(glm::radians(yaw)),
                              distance * std::sin(glm::radians(pitch)),
                              distance * cos_pitch * std::cos(glm::radians(yaw)));
  }

  [[nodiscard]] glm::vec3 GetForward() const { return glm::normalize(target - GetPosition()); }

  [[nodiscard]] glm::mat4 GetViewMatrix() const {
    return glm::lookAt(GetPosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
  }

  [[nodiscard]] glm::mat4 GetProjectionMatrix() const {
    return glm::perspective(glm::radians(fov), aspect, near_plane, far_plane);
  }

  [[nodiscard]] glm::mat4 GetProjectionView() const { return GetProjectionMatrix() * GetViewMatrix(); }

  /// @brief Rotate around the target by mouse deltas (degrees-based).
  void Orbit(float dx, float dy, float sensitivity = 0.3f) {
    yaw -= dx * sensitivity;    // drag right -> orbit left (view pans right)
    pitch -= dy * sensitivity;  // drag up -> camera rises (looks down more)
    pitch = glm::clamp(pitch, -89.0f, 89.0f);
  }

  /// @brief Move the target along the camera's right/up axes.
  void Pan(float dx, float dy) {
    const glm::vec3 forward = GetForward();
    const glm::vec3 right   = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 up      = glm::normalize(glm::cross(right, forward));

    const float scale = distance * 0.0015f;
    target -= right * (dx * scale);
    target += up * (dy * scale);
  }

  /// @brief Change the orbit distance (positive delta zooms in).
  void Zoom(float delta) {
    distance -= delta * distance * 0.1f;
    distance = glm::clamp(distance, 0.1f, 10000.0f);
  }

  void Reset() {
    target   = glm::vec3(0.0f);
    yaw      = 45.0f;
    pitch    = 30.0f;
    distance = 8.0f;
    fov      = 45.0f;
  }
};

}  // namespace MEngine
