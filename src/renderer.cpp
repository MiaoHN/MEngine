#include "renderer.hpp"

#include <glad/glad.h>

#include "command.hpp"
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

  cmd->GetShader()->Bind();

  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

}  // namespace MEngine