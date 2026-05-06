#include "render/render_pipeline.hpp"

#include "render/rhi/rhi.hpp"
#include "render/shader.hpp"

namespace MEngine {

RenderPipeline::RenderPipeline() {}

RenderPipeline::~RenderPipeline() {}

void RenderPipeline::SetVertexArray(std::unique_ptr<IVertexArrayBackend> vao) { vao_ = std::move(vao); }

void RenderPipeline::SetShader(Ref<Shader> shader) { shader_ = shader; }

void RenderPipeline::Execute() {
  shader_->Bind();
  vao_->Bind();
  if (const auto *rhi = GetActiveRHI(); rhi) {
    rhi->DrawIndexedTriangles(vao_->GetCount());
  }
  vao_->Unbind();
  shader_->Unbind();
}

}  // namespace MEngine