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

  // Free-fly state (WASD + right-drag look).
  bool      fly_mode     = false;
  glm::vec3 fly_position{0.0f, 1.0f, 8.0f};
  float     fly_speed    = 5.0f;

  [[nodiscard]] bool IsFlyMode() const { return fly_mode; }

  /// @brief Switches between orbit and free-fly. Both directions are seamless:
  /// entering fly keeps the current eye + look direction, and exiting fly keeps
  /// the current eye + look direction by re-deriving the orbit target.
  void SetFlyMode(bool fly) {
    if (fly && !fly_mode) {
      // Enter fly: start from the current orbit eye, keep the look direction.
      fly_position      = GetPosition();
      const glm::vec3 d = glm::normalize(target - fly_position);
      yaw               = glm::degrees(std::atan2(d.x, d.z));
      pitch             = glm::degrees(std::asin(glm::clamp(d.y, -1.0f, 1.0f)));
    } else if (!fly && fly_mode) {
      // Exit fly: keep the current eye + look direction by re-deriving the
      // orbit target along the same view ray (retaining the orbit distance).
      const glm::vec3 forward = ForwardFromAngles();
      target                  = fly_position + forward * distance;
      yaw += 180.0f;
      pitch = -pitch;
    }
    fly_mode = fly;
  }

  /// @brief Unit direction encoded by the current yaw/pitch.
  [[nodiscard]] glm::vec3 ForwardFromAngles() const {
    const float cos_pitch = std::cos(glm::radians(pitch));
    return glm::normalize(glm::vec3(cos_pitch * std::sin(glm::radians(yaw)),
                                    std::sin(glm::radians(pitch)),
                                    cos_pitch * std::cos(glm::radians(yaw))));
  }

  /// @brief Camera eye position (orbit-derived, or free-fly position).
  [[nodiscard]] glm::vec3 GetPosition() const {
    if (fly_mode) {
      return fly_position;
    }
    return target + distance * ForwardFromAngles();
  }

  [[nodiscard]] glm::vec3 GetForward() const {
    return fly_mode ? ForwardFromAngles() : glm::normalize(target - GetPosition());
  }

  [[nodiscard]] glm::mat4 GetViewMatrix() const {
    if (fly_mode) {
      return glm::lookAt(fly_position, fly_position + ForwardFromAngles(), glm::vec3(0.0f, 1.0f, 0.0f));
    }
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

  /// @brief Rotate the free-fly camera by mouse deltas (look around).
  void LookAround(float dx, float dy, float sensitivity = 0.25f) {
    yaw += dx * sensitivity;    // drag right -> look right
    pitch -= dy * sensitivity;  // drag up (dy < 0) -> look up
    pitch = glm::clamp(pitch, -89.0f, 89.0f);
  }

  /// @brief Move the free-fly camera along its local axes (amounts are unit
  /// multipliers; `dt` scales by the fly speed).
  void MoveLocal(float forward_amount, float right_amount, float up_amount, float dt) {
    const glm::vec3 fwd   = ForwardFromAngles();
    const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 up    = glm::cross(right, fwd);
    fly_position += (fwd * forward_amount + right * right_amount + up * up_amount) * fly_speed * dt;
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
    target       = glm::vec3(0.0f);
    yaw          = 45.0f;
    pitch        = 30.0f;
    distance     = 8.0f;
    fov          = 45.0f;
    fly_mode     = false;
    fly_position = glm::vec3(0.0f, 1.0f, 8.0f);
  }
};

}  // namespace MEngine
