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

#include <glad/glad.h>

#include <memory>
#include <vector>

namespace MEngine {

class RenderPipeline;

class RenderPass {
 public:
  RenderPass();
  ~RenderPass();

  void AddPipeline(std::shared_ptr<RenderPipeline> pipeline);

  void Begin();

  void End();

  void Execute();

 private:
  GLuint fb_;

  std::vector<std::shared_ptr<RenderPipeline>> pipelines_;
};

}  // namespace MEngine