#include "render/renderer.hpp"

#include <glad/glad.h>

#include "core/command.hpp"
#include "render/gl.hpp"
#include "render/shader.hpp"

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

  vertex_array_ = std::make_shared<GL::VertexArray>();
  vertex_array_->SetVertexBuffer(vertex_buffer);
  vertex_array_->SetIndexBuffer(index_buffer);
}

Renderer::~Renderer() {}

void Renderer::Run(Command* command) {
  if (command->IsCancelled()) {
    return;
  }
  if (command->GetType() != Command::Type::Render) {
    logger_->error("Invalid command type");
    return;
  }

  RenderCommand* cmd = dynamic_cast<RenderCommand*>(command);

  auto shader  = cmd->GetRenderInfo().shader;
  auto texture = cmd->GetRenderInfo().texture;

  shader->Bind();
  texture->Bind();

  shader->SetUniform("model", cmd->GetModelMatrix());
  shader->SetUniform("proj_view", cmd->GetProjectionView());
  shader->SetUniform("texture1", 0);

  vertex_array_->Bind();

  glDrawElements(GL_TRIANGLES, vertex_array_->GetCount(), GL_UNSIGNED_INT, 0);
}

}  // namespace MEngine