/**
 * @file render_pipeline.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-05-23
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include "core/common.hpp"

#include "render/rhi/resource_backend.hpp"

namespace MEngine {

class Shader;

class RenderPipeline {
 public:
  RenderPipeline();
  ~RenderPipeline();

  void SetVertexArray(std::unique_ptr<IVertexArrayBackend> vao);
  void SetShader(Ref<Shader> shader);

  Ref<Shader>          GetShader() { return shader_; }

  void Execute();

 private:
  std::unique_ptr<IVertexArrayBackend> vao_;
  Ref<Shader>                         shader_;
};

}  // namespace MEngine
