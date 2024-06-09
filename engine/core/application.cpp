#include "core/application.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/command.hpp"
#include "core/input.hpp"
#include "core/script_engine.hpp"
#include "render/frame_buffer.hpp"
#include "render/gl.hpp"
#include "render/renderer.hpp"
#include "render/shader.hpp"
#include "scene/camera.hpp"
#include "scene/component.hpp"
#include "scene/scene.hpp"

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
  prev_time_   = glfwGetTime();
  frame_time_  = glfwGetTime();
  frame_count_ = 0;
  fps_         = 0;

  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window_ = glfwCreateWindow(1600, 900, "MEngine", nullptr, nullptr);

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

  logger_->info("Application initialized");
}

Application::~Application() {
  if (window_) {
    glfwDestroyWindow(window_);
  }
  glfwTerminate();
  logger_->info("Application terminated");
}

void Application::Initialize() {
  // NOTE: This is a default implementation.
}

void Application::OnUpdate(float dt) {
  // NOTE: This is a default implementation.
}

void Application::Run() {
  while (!glfwWindowShouldClose(window_)) {
    float dt = GetDeltaTime();

    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    OnUpdate(dt);

    glfwSwapBuffers(window_);

    glfwPollEvents();
  }
}

float Application::GetDeltaTime() {
  float current_time = static_cast<float>(glfwGetTime());
  float delta_time   = current_time - prev_time_;
  prev_time_         = current_time;

  frame_count_++;

  if (current_time - frame_time_ >= 1.0f) {
    fps_         = frame_count_;
    frame_count_ = 0;
    frame_time_  = current_time;
  }

  return delta_time;
}

}  // namespace MEngine