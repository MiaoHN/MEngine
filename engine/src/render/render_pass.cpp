#include "render/render_pass.hpp"

#include "render/render_pipeline.hpp"

namespace MEngine {

RenderPass::RenderPass() { glGenFramebuffers(1, &fb_); }

RenderPass::~RenderPass() { glDeleteFramebuffers(1, &fb_); }

void RenderPass::AddPipeline(std::shared_ptr<RenderPipeline> pipeline) { pipelines_.push_back(pipeline); }

void RenderPass::Begin() {
  glBindFramebuffer(GL_FRAMEBUFFER, fb_);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void RenderPass::End() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void RenderPass::Execute() {
  for (auto &pipeline : pipelines_) {
    pipeline->Execute();
  }
}

}  // namespace MEngine
