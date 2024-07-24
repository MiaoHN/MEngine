/**
 * @file render_context.hpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-06-17
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <glad/glad.h>

#include <memory>

namespace MEngine {

class RenderPipeline;

class RenderContext {
 public:
  RenderContext();
  ~RenderContext();

  void AddPipeline(std::shared_ptr<RenderPipeline> pipeline);

  void Begin();

  void End();

  void Execute();

  GLuint GetFramebuffer() { return fb_; }

 private:
  GLuint fb_;

  std::vector<std::shared_ptr<RenderPipeline>> pipelines_;
};

}  // namespace MEngine
