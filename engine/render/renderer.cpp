#include "render/renderer.hpp"

#include <glad/glad.h>

#include "core/command.hpp"
#include "render/gl.hpp"
#include "render/shader.hpp"

namespace MEngine {

Renderer::Renderer() { logger_ = Logger::Get("Renderer"); }

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

  glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  auto shader  = cmd->GetRenderInfo().shader;
  auto texture = cmd->GetRenderInfo().texture;

  texture->Bind();

  shader->SetUniform("model", cmd->GetModelMatrix());
  shader->SetUniform("view_proj", cmd->GetViewProjectionMatrix());
  shader->SetUniform("texture1", 0);

  auto vertex_array = cmd->GetRenderInfo().vertex_array;

  vertex_array->Bind();

  glDrawElements(GL_TRIANGLES, vertex_array->GetCount(), GL_UNSIGNED_INT, 0);
}

}  // namespace MEngine