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

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/command.hpp"
#include "core/logger.hpp"
#include "core/task_handler.hpp"

namespace MEngine {

class OrthographicCamera : public TaskHandler {
 public:
  OrthographicCamera(float left, float right, float bottom, float top) {
    logger_ = Logger::Get("OrthographicCamera");

    projection_ = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    view_       = glm::mat4(1.0f);

    position_ = glm::vec3(0.0f);

    rotation_ = 0.0f;

    RecalculateViewMatrix();
  }

  ~OrthographicCamera() = default;

  void Run(Command* cmd) override {
    switch (cmd->GetType()) {
      case Command::Type::Move: {
        auto move_cmd = static_cast<MoveCommand*>(cmd);
        position_ += move_cmd->GetDirection() * move_cmd->GetVelocity();
        RecalculateViewMatrix();
        break;
      }
      case Command::Type::Rotate: {
        auto rotate_cmd = static_cast<RotateCommand*>(cmd);
        rotation_ += rotate_cmd->GetAngle();
        RecalculateViewMatrix();
        break;
      }
      default:
        break;
    }
  }

  void OnWindowResize(float width, float height) {
    aspect_ratio_ = width / height;
    SetProjection(-aspect_ratio_ * zoom_level_, aspect_ratio_ * zoom_level_,
                  -zoom_level_, zoom_level_);
  }

  void OnMouseScroll(float y_offset) {
    zoom_level_ += y_offset;
    if (zoom_level_ < 0.25f) zoom_level_ = 0.25f;
    SetProjection(-aspect_ratio_ * zoom_level_, aspect_ratio_ * zoom_level_,
                  -zoom_level_, zoom_level_);
  }

  void SetProjection(float left, float right, float bottom, float top) {
    projection_ = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
  }

  void SetPosition(const glm::vec3& position) { position_ = position; }

  void SetRotation(float rotation) { rotation_ = rotation; }

  const glm::mat4& GetViewMatrix() const { return view_; }

  const glm::mat4& GetProjectionMatrix() const { return projection_; }

  glm::mat4 GetProjectionView() { return projection_ * view_; }

  const glm::vec3& GetPosition() const { return position_; }

  float GetRotation() const { return rotation_; }

  void RecalculateViewMatrix() {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), position_) *
                          glm::rotate(glm::mat4(1.0f), glm::radians(rotation_),
                                      glm::vec3(0, 0, 1));

    view_ = glm::inverse(transform);
  }

 private:
  glm::mat4 view_;
  glm::mat4 projection_;
  glm::vec3 position_;
  float     rotation_;

  float aspect_ratio_ = 0.0f;
  float zoom_level_   = 1.0f;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine
