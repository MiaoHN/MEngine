#include "render/render_pass.hpp"

#include "render/render_pipeline.hpp"
#include "render/rhi/rhi.hpp"

namespace MEngine {

RenderPass::RenderPass() {
  if (const auto *rhi = GetActiveRHI(); rhi) {
    fb_ = rhi->CreateFramebuffer();
  }
}

RenderPass::~RenderPass() {
  if (const auto *rhi = GetActiveRHI(); rhi) {
    rhi->DestroyFramebuffer(fb_);
  }
}

void RenderPass::AddPipeline(Ref<RenderPipeline> pipeline) { pipelines_.push_back(pipeline); }

void RenderPass::Begin() {
  if (const auto *rhi = GetActiveRHI(); rhi) {
    rhi->BindFramebuffer(fb_);
    rhi->ClearBoundFramebufferColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
  }
}

void RenderPass::End() {
  if (const auto *rhi = GetActiveRHI(); rhi) {
    rhi->BindFramebuffer(0);
  }
}

void RenderPass::Execute() {
  for (auto &pipeline : pipelines_) {
    pipeline->Execute();
  }
}

}  // namespace MEngine
