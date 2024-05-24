/**
 * @file render_pipeline.hpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-05-23
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <glad/glad.h>

#include <memory>

namespace MEngine {

namespace GL {
class VertexArray;
}

class Shader;

class RenderPipeline {
 public:
  RenderPipeline();
  ~RenderPipeline();

  void SetVertexArray(std::shared_ptr<GL::VertexArray> vao);
  void SetShader(std::shared_ptr<Shader> shader);

  std::shared_ptr<Shader>          GetShader() { return shader_; }
  std::shared_ptr<GL::VertexArray> GetVertexArray() { return vao_; }

  void Execute();

 private:
  std::shared_ptr<GL::VertexArray> vao_;
  std::shared_ptr<Shader>          shader_;
};

}  // namespace MEngine
