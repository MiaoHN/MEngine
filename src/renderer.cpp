#include "renderer.hpp"

#include <glad/glad.h>

#include "command.hpp"
#include "gl.hpp"
#include "shader.hpp"

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

  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  auto shader = cmd->GetShader();

  shader->SetUniform("model", cmd->GetModelMatrix());
  shader->SetUniform("view_proj", cmd->GetViewProjectionMatrix());

  shader->Bind();
  cmd->GetVertexArray()->Bind();

  glDrawArrays(GL_TRIANGLES, 0, 3);
}

}  // namespace MEngine