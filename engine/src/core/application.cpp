#include "core/application.hpp"
#include "core/logger.hpp"
namespace MEngine {

static Application *s_app;

Application *Application::GetInstance() { return s_app; }

Application::Application() {
  if (s_app) {
    LOG_ERROR("Application") << "Application already exists";
    exit(-1);
  }
  s_app = this;

  LOG_INFO("Application") << "Application started";

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
    LOG_ERROR("Application") << "Failed to create GLFW window";
    glfwTerminate();
    exit(-1);
  }

  glfwMakeContextCurrent(window_);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    LOG_FATAL("Application") << "Failed to initialize GLAD";
    exit(-1);
  }

  LOG_INFO("Application") << "OpenGL Version: " << glGetString(GL_VERSION);
  LOG_INFO("Application") << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION);
  LOG_INFO("Application") << "Vendor: " << glGetString(GL_VENDOR);
  LOG_INFO("Application") << "Renderer: " << glGetString(GL_RENDERER);
  LOG_INFO("Application") << "Application initialized";
}

Application::~Application() {
  if (window_) {
    glfwDestroyWindow(window_);
  }
  glfwTerminate();
  LOG_INFO("Application") << "Application terminated";
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