#include "core/application.hpp"

#include <fstream>

#include "core/logger.hpp"
#include "render/asset_manager.hpp"
#include "utils/profiler.h"

namespace MEngine {

static Application *s_app;

std::string Application::startup_scene_path_;
GraphicsAPI Application::startup_api_ = GraphicsAPI::OpenGL;
int         Application::max_frames_  = 0;  // 0 = run until the window closes
bool        Application::window_hidden_ = false;
int         Application::capture_frame_  = 0;  // 0 = disabled
std::string Application::capture_out_path_ = "capture.ppm";

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

  // Total rendered frames (frame_count_ is the rolling FPS counter and resets
  // every second — never use it for budgets or one-shot frame triggers).
  int total_frames = 0;

  while (!glfwWindowShouldClose(window_)) {
    PROFILER_SCOPE("One Frame");

    const float dt = GetDeltaTime();
    ++total_frames;

    rhi_->BeginFrame(glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));

    OnUpdate(dt);

    rhi_->EndFrame(window_);

    glfwPollEvents();

    // Unattended verification: save the backbuffer as PPM on the requested
    // frame (read before the swap, from the still-current default framebuffer).
    if (capture_frame_ > 0 && total_frames == capture_frame_) {
      int width = 0;
      int height = 0;
      glfwGetFramebufferSize(window_, &width, &height);
      std::vector<unsigned char> rgb;
      if (rhi_ && width > 0 && height > 0 && rhi_->ReadBackBuffer(width, height, rgb)) {
        std::ofstream file(capture_out_path_, std::ios::binary);
        if (file.is_open()) {
          file << "P6\n" << width << " " << height << "\n255\n";
          // OpenGL rows are bottom-up; flip so the PPM is top-down.
          for (int row = height - 1; row >= 0; --row) {
            file.write(reinterpret_cast<const char *>(rgb.data()) + static_cast<size_t>(row) * width * 3,
                       static_cast<std::streamsize>(width) * 3);
          }
          LOG_INFO("Application") << "Captured frame " << capture_frame_ << " to " << capture_out_path_ << " ("
                                  << width << "x" << height << ")";
        } else {
          LOG_ERROR("Application") << "Failed to open capture file " << capture_out_path_;
        }
      } else {
        LOG_ERROR("Application") << "Backbuffer readback unavailable or failed";
      }
    }

    if (max_frames_ > 0 && total_frames >= max_frames_) {
      LOG_INFO("Application") << "Frame budget reached (" << total_frames << " frames); exiting";
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