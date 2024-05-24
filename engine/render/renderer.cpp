#include "render/renderer.hpp"

#include <glad/glad.h>

#include "core/command.hpp"
#include "render/gl.hpp"
#include "render/render_pipeline.hpp"
#include "render/shader.hpp"
#include "scene/component.hpp"

namespace MEngine {

Renderer::Renderer() {
  logger_ = Logger::Get("Renderer");

  float vertices[] = {
      // positions        // texture coords
      0.5f,  0.5f,  0.0f, 1.0f, 1.0f,  // top right
      0.5f,  -0.5f, 0.0f, 1.0f, 0.0f,  // bottom right
      -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // bottom left
      -0.5f, 0.5f,  0.0f, 0.0f, 1.0f   // top left
  };
  unsigned int indices[] = {
      0, 1, 3,  // first triangle
      1, 2, 3   // second triangle
  };

  auto vertex_buffer = std::make_shared<GL::VertexBuffer>();
  vertex_buffer->SetData(vertices, sizeof(vertices));
  vertex_buffer->AddLayout({
      {GL::ShaderDataType::Float3, "aPos"},
      {GL::ShaderDataType::Float2, "aTexCoord"},
  });

  auto index_buffer = std::make_shared<GL::IndexBuffer>();
  index_buffer->SetData(indices, 6);

  auto vertex_array = std::make_shared<GL::VertexArray>();
  vertex_array->SetVertexBuffer(vertex_buffer);
  vertex_array->SetIndexBuffer(index_buffer);

  // TODO: 默认 shader 怎么存放
  auto shader = std::make_shared<Shader>("res/shaders/default_vert.glsl",
                                         "res/shaders/default_frag.glsl");
  shader->Bind();
  shader->SetUniform("texture1", 0);
  shader->Unbind();

  pipeline_ = std::make_shared<RenderPipeline>();

  pipeline_->SetVertexArray(vertex_array);
  pipeline_->SetShader(shader);
}

Renderer::~Renderer() {}

void Renderer::RenderSprite(Sprite& sprite, const glm::mat4& proj_view) {
  auto shader  = pipeline_->GetShader();
  auto texture = sprite.texture;

  shader->Bind();
  texture->Bind();

  shader->SetUniform("model", sprite.GetModelMatrix());
  shader->SetUniform("proj_view", proj_view);
  shader->SetUniform("texture1", 0);

  pipeline_->Execute();
}

}  // namespace MEngine