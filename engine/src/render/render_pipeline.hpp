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

#include "core/common.hpp"

namespace MEngine {

namespace GL {
class VertexArray;
}

class Shader;

class RenderPipeline {
 public:
  RenderPipeline();
  ~RenderPipeline();

  void SetVertexArray(Ref<GL::VertexArray> vao);
  void SetShader(Ref<Shader> shader);

  Ref<Shader>          GetShader() { return shader_; }
  Ref<GL::VertexArray> GetVertexArray() { return vao_; }

  void Execute();

 private:
  Ref<GL::VertexArray> vao_;
  Ref<Shader>          shader_;
};

}  // namespace MEngine
