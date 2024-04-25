/**
 * @file camera.hpp
 * @author your name (you@domain.com)
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

namespace MEngine {

class OrthographicCamera {
 public:
  OrthographicCamera(float left, float right, float bottom, float top) {
    projection_ = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    view_       = glm::mat4(1.0f);

    position_ = glm::vec3(0.0f);

    rotation_ = 0.0f;

    RecalculateViewMatrix();
  }

  ~OrthographicCamera() = default;

  void SetPosition(const glm::vec3& position) { position_ = position; }

  void SetRotation(float rotation) { rotation_ = rotation; }

  const glm::mat4& GetViewMatrix() const { return view_; }

  const glm::mat4& GetProjectionMatrix() const { return projection_; }

  glm::mat4 GetViewProjectionMatrix() { return view_ * projection_; }

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
};

}  // namespace MEngine
