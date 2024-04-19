#include "application.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "command.hpp"
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

  // For test
  float vertices[] = {
      -0.5f, -0.5f, 0.0f,  // left
      0.5f,  -0.5f, 0.0f,  // right
      0.0f,  0.5f,  0.0f   // top
  };

  vertex_buffer_ = std::make_shared<GL::VertexBuffer>();
  vertex_buffer_->SetData(vertices, sizeof(vertices));
  vertex_buffer_->AddLayout({{GL::ShaderDataType::Float3, "aPos"}});

  vertex_array_ = std::make_shared<GL::VertexArray>();
  vertex_array_->SetVertexBuffer(vertex_buffer_);
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
    render_command.SetVertexArray(vertex_array_);
    renderer_->Run(&render_command);

    glfwSwapBuffers(window_);

    glfwPollEvents();
  }
}

}  // namespace MEngine