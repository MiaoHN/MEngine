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

  // --- 2D (orthographic) view mode -----------------------------------------
  // Used for scenes whose primary camera is orthographic: the editor looks
  // straight down -Z at the XY plane, pans in screen space and zooms by
  // changing the orthographic half-height instead of the orbit distance.
  bool      view_2d    = false;
  float     ortho_size = 6.0f;  // half-height of the visible area, world units
  glm::vec2 view_center{0.0f};  // XY point the 2D view is centered on
  float     view_2d_depth = 10.0f;  // eye distance along +Z (keeps sprites in front)
  int       viewport_height = 900;  // needed to convert mouse pixels to world units

  [[nodiscard]] bool Is2D() const { return view_2d; }

  /// @brief Enters/leaves the 2D view. Entering keeps the XY position of the
  /// current focus point so the switch never jumps.
  void Set2D(bool enabled) {
    if (enabled == view_2d) {
      return;
    }
    if (enabled) {
      // Focus the same world point the orbit camera was looking at.
      view_center = glm::vec2(target.x, target.y);
    } else {
      target = glm::vec3(view_center.x, view_center.y, 0.0f);
      yaw    = 45.0f;
      pitch  = 30.0f;
    }
    view_2d = enabled;
  }

  /// @brief Moves the 2D view center by a mouse delta in PIXELS.
  void Pan2D(float dx, float dy) {
    const float world_per_pixel = (2.0f * ortho_size) / static_cast<float>(std::max(1, viewport_height));
    view_center.x -= dx * world_per_pixel;
    view_center.y += dy * world_per_pixel;  // screen y is down, world y is up
  }

  /// @brief Zooms the 2D view (positive wheel delta zooms in).
  void Zoom2D(float delta) {
    ortho_size = glm::clamp(ortho_size * std::pow(0.9f, delta), 0.25f, 500.0f);
  }

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
    if (view_2d) {
      return glm::vec3(view_center.x, view_center.y, view_2d_depth);
    }
    if (fly_mode) {
      return fly_position;
    }
    return target + distance * ForwardFromAngles();
  }

  [[nodiscard]] glm::vec3 GetForward() const {
    return fly_mode ? ForwardFromAngles() : glm::normalize(target - GetPosition());
  }

  [[nodiscard]] glm::mat4 GetViewMatrix() const {
    if (view_2d) {
      // Straight down -Z at the XY plane, +Y up.
      const glm::vec3 eye    = GetPosition();
      const glm::vec3 centre = glm::vec3(view_center.x, view_center.y, 0.0f);
      return glm::lookAt(eye, centre, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    if (fly_mode) {
      return glm::lookAt(fly_position, fly_position + ForwardFromAngles(), glm::vec3(0.0f, 1.0f, 0.0f));
    }
    return glm::lookAt(GetPosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
  }

  [[nodiscard]] glm::mat4 GetProjectionMatrix() const {
    if (view_2d) {
      const float half_width = aspect * ortho_size;
      return glm::ortho(-half_width, half_width, -ortho_size, ortho_size, near_plane, far_plane);
    }
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

  /// @brief Move the target along the camera's right/up axes (or pan the 2D view
  /// when in 2D mode).
  void Pan(float dx, float dy) {
    if (view_2d) {
      Pan2D(dx, dy);
      return;
    }
    const glm::vec3 forward = GetForward();
    const glm::vec3 right   = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 up      = glm::normalize(glm::cross(right, forward));

    const float scale = distance * 0.0015f;
    target -= right * (dx * scale);
    target += up * (dy * scale);
  }

  /// @brief Change the orbit distance (positive delta zooms in). In 2D mode the
  /// orthographic size is scaled instead.
  void Zoom(float delta) {
    if (view_2d) {
      Zoom2D(delta);
      return;
    }
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
    view_2d      = false;
    ortho_size   = 6.0f;
    view_center  = glm::vec2(0.0f);
  }
};

}  // namespace MEngine
