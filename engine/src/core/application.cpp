#include "core/application.hpp"
#include "core/logger.hpp"
#include "render/asset_manager.hpp"
#include "utils/profiler.h"

namespace MEngine {

static Application *s_app;

std::string Application::startup_scene_path_;
GraphicsAPI Application::startup_api_ = GraphicsAPI::OpenGL;
int         Application::max_frames_  = 0;  // 0 = run until the window closes
bool        Application::window_hidden_ = false;

Application *Application::GetInstance() { return s_app; }

Application::Application(GraphicsAPI api) : graphics_api_(api) {
  if (s_app) {
    LOG_ERROR("Application") << "Application already exists";
    exit(-1);
  }
  s_app = this;

  // Shared asset root (single source of truth for shaders / textures / ...).
  AssetManager::Instance().SetAssetRoot("assets");

  LOG_INFO("Application") << "Application started";

  // NOTE: ImGui context ownership lives with the UI application (Editor), not
  // with the engine Application, so the engine itself never needs Dear ImGui.

  prev_time_   = static_cast<float>(glfwGetTime());
  frame_time_  = static_cast<float>(glfwGetTime());
  frame_count_ = 0;
  fps_         = 0;

  if (!glfwInit()) {
    LOG_FATAL("Application") << "Failed to initialize GLFW";
    exit(-1);
  }
  LOG_DEBUG("Application") << "GLFW initialized";

  rhi_ = CreateRHI(graphics_api_);
  if (!rhi_) {
    LOG_FATAL("Application") << "Failed to create RHI";
    exit(-1);
  }

  SetActiveRHI(rhi_);

  rhi_->SetupWindowHints();

  // `--hidden` keeps the window invisible (headless-style smoke runs still
  // render into the default framebuffer and swap normally).
  if (window_hidden_) {
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  }

  window_ = glfwCreateWindow(1600, 900, "MEngine", nullptr, nullptr);

  if (!window_) {
    LOG_ERROR("Application") << "Failed to create GLFW window";
    glfwTerminate();
    exit(-1);
  }

  int window_width  = 0;
  int window_height = 0;
  glfwGetFramebufferSize(window_, &window_width, &window_height);
  LOG_DEBUG("Application") << "Window created (framebuffer " << window_width << "x" << window_height << ")";

  if (!rhi_->Initialize(window_)) {
    LOG_FATAL("Application") << "Failed to initialize render backend";
    exit(-1);
  }

  LOG_INFO("Application") << "Application initialized";
}

Application::~Application() {
  rhi_.reset();

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
  (void)dt;
  // NOTE: This is a default implementation.
}

void Application::Run() {
  PROFILER_FUNCTION();
  while (!glfwWindowShouldClose(window_)) {
    PROFILER_SCOPE("One Frame");

    const float dt = GetDeltaTime();

    rhi_->BeginFrame(glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));

    OnUpdate(dt);

    rhi_->EndFrame(window_);

    glfwPollEvents();

    // Unattended runs: exit after the requested frame budget.
    if (max_frames_ > 0 && frame_count_ >= max_frames_) {
      LOG_INFO("Application") << "Frame budget reached (" << frame_count_ << " frames); exiting";
      break;
    }
  }
}

float Application::GetDeltaTime() {
  const auto  current_time = static_cast<float>(glfwGetTime());
  const float delta_time   = current_time - prev_time_;
  prev_time_               = current_time;

  frame_count_++;

  if (current_time - frame_time_ >= 1.0f) {
    fps_         = frame_count_;
    frame_count_ = 0;
    frame_time_  = current_time;
  }

  return delta_time;
}

}  // namespace MEngine