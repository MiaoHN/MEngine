#include "application.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "command.hpp"
#include "renderer.hpp"
#include "scene.hpp"
#include "shader.hpp"
#include "task_dispatcher.hpp"
#include "task_handler.hpp"

namespace MEngine {

Application::Application() {
  logger_ = Logger::Get("Application");
  logger_->info("Application started");
  task_dispatcher_ = std::make_unique<TaskDispatcher>();
  scene_           = std::make_unique<Scene>();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  if (!glfwInit()) {
    logger_->error("Failed to initialize GLFW");
    glfwTerminate();
    exit(-1);
  }

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

  shader_ = std::make_shared<Shader>("res/shaders/test_vert.glsl",
                                     "res/shaders/test_frag.glsl");

  renderer_ = std::make_shared<Renderer>();
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
    RenderCommand render_command;
    render_command.SetShader(shader_);
    renderer_->Run(&render_command);

    glfwSwapBuffers(window_);

    glfwPollEvents();
  }
}

}  // namespace MEngine