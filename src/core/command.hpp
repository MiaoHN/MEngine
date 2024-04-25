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

  void SetVertexArray(std::shared_ptr<GL::VertexArray> vertex_array) {
    vertex_array_ = vertex_array;
  }

  void SetShader(std::shared_ptr<Shader> shader) { shader_ = shader; }

  std::shared_ptr<GL::VertexArray> GetVertexArray() const {
    return vertex_array_;
  }

  std::shared_ptr<Shader> GetShader() const { return shader_; }

  void SetModelMatrix(const glm::mat4& model_matrix) {
    model_matrix_ = model_matrix;
  }

  void SetViewProjectionMatrix(const glm::mat4& view_projection_matrix) {
    view_projection_matrix_ = view_projection_matrix;
  }

  const glm::mat4& GetModelMatrix() const { return model_matrix_; }

  const glm::mat4& GetViewProjectionMatrix() const {
    return view_projection_matrix_;
  }

 private:
  std::shared_ptr<GL::VertexArray> vertex_array_;
  std::shared_ptr<Shader>          shader_;

  glm::mat4 model_matrix_;
  glm::mat4 view_projection_matrix_;
};

}  // namespace MEngine
