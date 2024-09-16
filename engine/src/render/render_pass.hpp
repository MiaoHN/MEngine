/**
 * @file render_pass.hpp
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

class RenderPipeline;

class RenderPass {
 public:
  RenderPass();
  ~RenderPass();

  void AddPipeline(Ref<RenderPipeline> pipeline);

  void Begin();

  void End();

  void Execute();

  GLuint GetFramebuffer() { return fb_; }

 private:
  GLuint fb_;
  GLuint getFramebuffer() const;
  GLuint getTexture() const;

 private:
  GLuint fbo;
  GLuint texture;
  GLenum clearBits;
  GLenum attachment;

  std::vector<Ref<RenderPipeline>> pipelines_;
};

}  // namespace MEngine