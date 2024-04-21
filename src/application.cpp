#include "application.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "command.hpp"
#include "component.hpp"
#include "gl.hpp"
#include "renderer.hpp"
#include "scene.hpp"
#include "shader.hpp"
#include "task_dispatcher.hpp"
#include "task_handler.hpp"

namespace MEngine {

Application::Application() {
  logger_ = Logger::Get("Application");
  logger_->info("Application started");

  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window_ = glfwCreateWindow(800, 600, "MEngine", nullptr, nullptr);

  if (!window_) {
    logger_->error("Failed to create GLFW window");
    glfwTerminate();
    exit(-1);
  }

  glfwMakeContextCurrent(window_);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    logger_->critical("Failed to initialize GLAD!");
    exit(-1);
  }

  task_dispatcher_ = std::make_unique<TaskDispatcher>();
  scene_           = std::make_unique<Scene>();
  renderer_        = std::make_shared<Renderer>();
}

Application::~Application() {
  if (window_) {
    glfwDestroyWindow(window_);
  }
  glfwTerminate();
  logger_->info("Application terminated");
}

void Application::Run() {
  while (!glfwWindowShouldClose(window_)) {
    // handle logic
    Command test_command(Command::Type::Logic);
    task_dispatcher_->Run(&test_command);

    // handle render
    auto entities = scene_->GetAllEntitiesWith<RenderInfo>();
    for (auto& entity : entities) {
      RenderInfo&   render_info = entity.GetComponent<RenderInfo>();
      RenderCommand render_command;
      render_command.SetShader(render_info.shader);
      render_command.SetVertexArray(render_info.vertex_array);
      renderer_->Run(&render_command);
    }

    glfwSwapBuffers(window_);

    glfwPollEvents();
  }
}

}  // namespace MEngine