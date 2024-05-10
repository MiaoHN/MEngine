/**
 * @file command.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-17
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>

#include "scene/component.hpp"

namespace MEngine {

namespace GL {
class VertexArray;
};

class Shader;

/**
 * @brief Command class is a utility class for parsing command line arguments.
 * @warning it's better to create it on the stack.
 *
 */
class Command {
 public:
  enum class Type {
    Logic,
    Move,
    Rotate,
    Render,
  };

  Command(Type type) : type_(type), is_cancelled_(false) {}

  virtual ~Command() = default;

  const bool IsCancelled() const { return is_cancelled_; }

  void Cancel() { is_cancelled_ = true; }

  const Type GetType() const { return type_; }

 private:
  Type type_;
  bool is_cancelled_;
};

class RenderCommand : public Command {
 public:
  RenderCommand() : Command(Type::Render) {}

  ~RenderCommand() {}

  void SetRenderInfo(const RenderInfo& render_info) {
    render_info_ = render_info;
  }

  RenderInfo& GetRenderInfo() { return render_info_; }

  void SetModelMatrix(const glm::mat4& model_matrix) {
    model_matrix_ = model_matrix;
  }

  void SetViewProjectionMatrix(const glm::mat4& view_projection_matrix) {
    view_projection_matrix_ = view_projection_matrix;
  }

  const glm::mat4& GetModelMatrix() const { return model_matrix_; }

  const glm::mat4& GetProjectionView() const {
    return view_projection_matrix_;
  }

 private:
  RenderInfo render_info_;

  glm::mat4 model_matrix_;
  glm::mat4 view_projection_matrix_;
};

class MoveCommand : public Command {
 public:
  MoveCommand(const glm::vec3& direction, const float velocity)
      : Command(Type::Move), direction_(direction), velocity_(velocity) {}

  ~MoveCommand() {}

  const glm::vec3& GetDirection() const { return direction_; }
  const glm::vec3& GetVelocity() const { return velocity_; }

 private:
  glm::vec3 direction_;
  glm::vec3 velocity_;
};

class RotateCommand : public Command {
 public:
  RotateCommand(float angle) : Command(Type::Rotate), angle_(angle) {}

  ~RotateCommand() {}

  const float GetAngle() const { return angle_; }

 private:
  float angle_;
};

}  // namespace MEngine
