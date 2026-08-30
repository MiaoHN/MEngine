#include "core/application.hpp"
#include "core/logger.hpp"
#include "render/asset_manager.hpp"
#include "utils/profiler.h"

namespace MEngine {

static Application *s_app;

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

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::SetCurrentContext(ImGui::GetCurrentContext());
  ImGui::StyleColorsDark();

  prev_time_   = static_cast<float>(glfwGetTime());
  frame_time_  = static_cast<float>(glfwGetTime());
  frame_count_ = 0;
  fps_         = 0;

  glfwInit();

  rhi_ = CreateRHI(graphics_api_);
  if (!rhi_) {
    LOG_FATAL("Application") << "Failed to create RHI";
    exit(-1);
  }

  SetActiveRHI(rhi_);

  rhi_->SetupWindowHints();

  window_ = glfwCreateWindow(1600, 900, "MEngine", nullptr, nullptr);

  if (!window_) {
    LOG_ERROR("Application") << "Failed to create GLFW window";
    glfwTerminate();
    exit(-1);
  }

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