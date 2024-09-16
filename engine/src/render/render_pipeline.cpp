#include "render/render_pipeline.hpp"

#include <glad/glad.h>

#include <memory>

#include "render/gl.hpp"
#include "render/shader.hpp"

namespace MEngine {

RenderPipeline::RenderPipeline() {}

RenderPipeline::~RenderPipeline() {}

void RenderPipeline::SetVertexArray(Ref<GL::VertexArray> vao) { vao_ = vao; }

void RenderPipeline::SetShader(Ref<Shader> shader) { shader_ = shader; }

void RenderPipeline::Execute() {
  shader_->Bind();
  vao_->Bind();
  glDrawElements(GL_TRIANGLES, vao_->GetCount(), GL_UNSIGNED_INT, nullptr);
  vao_->Unbind();
  shader_->Unbind();
}

}  // namespace MEngine