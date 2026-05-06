/**
 * @file render_pass.hpp
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

  unsigned int GetFramebuffer() { return fb_; }

 private:
  unsigned int fb_ = 0;

  std::vector<Ref<RenderPipeline>> pipelines_;
};

}  // namespace MEngine