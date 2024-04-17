#include "application.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "task_dispatcher.hpp"
#include "scene.hpp"

#include "command.hpp"

namespace MEngine {

Application::Application() {
  logger_ = Logger::Get("Application");
  logger_->info("Application started");
  task_dispatcher_ = std::make_unique<TaskDispatcher>();
  scene_  = std::make_unique<Scene>();

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
    glClear(GL_COLOR_BUFFER_BIT);

    Command test_command(Command::Type::Logic);
    task_dispatcher_->Do(test_command);

    glfwSwapBuffers(window_);

    glfwPollEvents();
  }
}

}  // namespace MEngine