#include "render/renderer.hpp"

#include "core/command.hpp"
#include "render/rhi/resource_backend.hpp"
#include "render/render_pass.hpp"
#include "render/render_pipeline.hpp"
#include "render/shader.hpp"
#include "scene/component.hpp"
#include "utils/profiler.h"

namespace MEngine {

Renderer::Renderer() {
  constexpr float vertices[] = {
      // positions        // texture coords
      0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  // top right
      0.5f,  -0.5f, 0.0f, 1.0f, 0.0f,  // bottom right
      -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // bottom left
      -0.5f, 0.5f,  0.0f, 0.0f, 1.0f   // top left
  };
  const unsigned int indices[] = {
      0, 1, 3,  // first triangle
      1, 2, 3   // second triangle
  };

  auto vertex_array = CreateVertexArrayBackend();
    vertex_array->SetVertexBuffer(vertices, sizeof(vertices),
                  {
                    {VertexAttributeType::Float3, "aPos"},
                    {VertexAttributeType::Float2, "aTexCoord"},
                  });
  vertex_array->SetIndexBuffer(indices, 6);

  // TODO: 默认 shader 怎么存放
  const auto shader = CreateRef<Shader>("res/shaders/default_vert.glsl", "res/shaders/default_frag.glsl");
  shader->Bind();
  shader->SetUniform("texture1", 0);
  MEngine::Shader::Unbind();

  pipeline_ = CreateRef<RenderPipeline>();

  pipeline_->SetVertexArray(std::move(vertex_array));
  pipeline_->SetShader(shader);

  pass_ = CreateRef<RenderPass>();
  pass_->AddPipeline(pipeline_);
}

Renderer::~Renderer() = default;

void Renderer::RenderSprite(Sprite2D &sprite, const glm::mat4 &proj_view) const {
  PROFILER_FUNCTION();

  static Ref<Texture> plain_texture = CreateRef<Texture>();
  if (!sprite.texture) {
    // 根据 sprite 颜色绘制纯色texture

    // 生成纯色纹理
    unsigned char color[4] = {static_cast<unsigned char>(sprite.color[0]), static_cast<unsigned char>(sprite.color[1]),
                              static_cast<unsigned char>(sprite.color[2]), static_cast<unsigned char>(sprite.color[3])};
    plain_texture->SetData(color, 1, 1);

    const auto shader = pipeline_->GetShader();

    shader->Bind();
    plain_texture->Bind();

    shader->SetUniform("model", sprite.GetModelMatrix());
    shader->SetUniform("proj_view", proj_view);
    shader->SetUniform("texture1", 0);

    pipeline_->Execute();
  } else {
    const auto shader  = pipeline_->GetShader();
    const auto texture = sprite.texture;

    shader->Bind();
    texture->Bind();

    shader->SetUniform("model", sprite.GetModelMatrix());
    shader->SetUniform("proj_view", proj_view);
    shader->SetUniform("texture1", 0);

    pipeline_->Execute();
  }
}

void Renderer::RenderSprite(AnimatedSprite2D &sprite, const glm::mat4 &proj_view) const {
  PROFILER_FUNCTION();

  const auto shader  = pipeline_->GetShader();
  const auto texture = sprite.texture;

  shader->Bind();
  texture->SetSubTexture(sprite.current_frame);

  shader->SetUniform("model", sprite.GetModelMatrix());
  shader->SetUniform("proj_view", proj_view);
  shader->SetUniform("texture1", 0);

  pipeline_->Execute();
  // pass_->Begin();

  // pass_->Execute();

  // pass_->End();

  // texture->Unbind();
}

unsigned int Renderer::GetFramebuffer() const { return pass_->GetFramebuffer(); }

}  // namespace MEngine