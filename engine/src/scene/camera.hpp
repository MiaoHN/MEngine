/**
 * @file camera.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-21
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/logger.hpp"
#include "scene/component.hpp"

namespace MEngine {

class OrthographicCamera {
 public:
  OrthographicCamera(Camera2D &camera_info) : camera_info_(camera_info) {
    logger_ = Logger::Get("OrthographicCamera");

    projection_ = glm::ortho(-camera_info_.aspect_ratio * camera_info_.zoom_level,
                             camera_info_.aspect_ratio * camera_info_.zoom_level, -camera_info_.zoom_level,
                             camera_info_.zoom_level, -1.0f, 1.0f);
    view_       = glm::mat4(1.0f);

    RecalculateViewMatrix();
  }

  ~OrthographicCamera() = default;

  void OnWindowResize(float width, float height) {
    camera_info_.aspect_ratio = width / height;
    SetProjection(-camera_info_.aspect_ratio * camera_info_.zoom_level,
                  camera_info_.aspect_ratio * camera_info_.zoom_level, -camera_info_.zoom_level,
                  camera_info_.zoom_level);
  }

  void OnMouseScroll(float y_offset) {
    camera_info_.zoom_level += y_offset;
    if (camera_info_.zoom_level < 0.25f) camera_info_.zoom_level = 0.25f;
    SetProjection(-camera_info_.aspect_ratio * camera_info_.zoom_level,
                  camera_info_.aspect_ratio * camera_info_.zoom_level, -camera_info_.zoom_level,
                  camera_info_.zoom_level);
  }

  void SetProjection(float left, float right, float bottom, float top) {
    projection_ = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
  }

  void SetPosition(const glm::vec3 &position) { camera_info_.position = position; }

  void SetRotation(float rotation) { camera_info_.rotation = rotation; }

  const glm::mat4 &GetViewMatrix() const { return view_; }

  const glm::mat4 &GetProjectionMatrix() const { return projection_; }

  glm::mat4 GetProjectionView() {
    RecalculateViewMatrix();
    return projection_ * view_;
  }

  const glm::vec3 &GetPosition() const { return camera_info_.position; }

  glm::vec3 &GetPosition() { return camera_info_.position; }

  const float &GetRotation() const { return camera_info_.rotation; }

  float &GetRotation() { return camera_info_.rotation; }

  void RecalculateViewMatrix() {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), camera_info_.position) *
                          glm::rotate(glm::mat4(1.0f), glm::radians(camera_info_.rotation), glm::vec3(0, 0, 1));

    view_ = glm::inverse(transform);
  }

  const float &GetZoomLevel() const { return camera_info_.zoom_level; }
  float       &GetZoomLevel() { return camera_info_.zoom_level; }
  const float &GetAspectRatio() const { return camera_info_.aspect_ratio; }
  float       &GetAspectRatio() { return camera_info_.aspect_ratio; }

  void SetZoomLevel(float zoom_level) {
    camera_info_.zoom_level = zoom_level;
    SetProjection(-camera_info_.aspect_ratio * camera_info_.zoom_level,
                  camera_info_.aspect_ratio * camera_info_.zoom_level, -camera_info_.zoom_level,
                  camera_info_.zoom_level);
  }
  void SetAspectRatio(float aspect_ratio) {
    camera_info_.aspect_ratio = aspect_ratio;
    SetProjection(-camera_info_.aspect_ratio * camera_info_.zoom_level,
                  camera_info_.aspect_ratio * camera_info_.zoom_level, -camera_info_.zoom_level,
                  camera_info_.zoom_level);
  }

  void SetPrimary(bool primary = true) { camera_info_.primary = primary; }

 private:
  glm::mat4 view_;
  glm::mat4 projection_;

  Camera2D &camera_info_;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine
