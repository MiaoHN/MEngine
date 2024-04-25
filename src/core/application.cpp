#include "core/application.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "scene/camera.hpp"
#include "core/command.hpp"
#include "scene/component.hpp"
#include "render/gl.hpp"
#include "render/renderer.hpp"
#include "scene/scene.hpp"
#include "core/input.hpp"
#include "render/shader.hpp"
#include "core/task_dispatcher.hpp"
#include "core/task_handler.hpp"

namespace MEngine {

static Application* s_app;

Application* Application::GetInstance() { return s_app; }

Application::Application() {
  if (s_app) {
    logger_->error("Application already exists");
    exit(-1);
  }
  s_app   = this;
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
  scene_           = std::make_shared<Scene>();
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

    if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
      glfwSetWindowShouldClose(window_, true);
    }

    // handle logic
    Command test_command(Command::Type::Logic);
    task_dispatcher_->Run(&test_command);

    // handle render
    auto entities = scene_->GetAllEntitiesWith<RenderInfo>();
    auto camera   = scene_->GetCamera();
    int  width, height;
    glfwGetFramebufferSize(window_, &width, &height);

    glViewport(0, 0, width, height);

    for (auto& entity : entities) {
      auto& transform = entity.GetComponent<Transform>();

      // rotate
      transform.rotation.z += 0.1f;

      RenderInfo&   render_info = entity.GetComponent<RenderInfo>();
      RenderCommand render_command;
      render_command.SetViewProjectionMatrix(camera->GetViewProjectionMatrix());
      render_command.SetModelMatrix(
          entity.GetComponent<Transform>().GetModelMatrix());
      render_command.SetShader(render_info.shader);
      render_command.SetVertexArray(render_info.vertex_array);
      renderer_->Run(&render_command);
    }

    glfwSwapBuffers(window_);

    glfwPollEvents();
  }
}

}  // namespace MEngine