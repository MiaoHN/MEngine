/**
 * @file application.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-16
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include "core/common.hpp"

#include "render/rhi/rhi.hpp"

#include "scene/entity.hpp"

struct GLFWwindow;

namespace MEngine {

class FrameBuffer;
class ScriptEngine;
class Scene;
class Renderer;
class IRHI;

/**
 * @brief Application class is the main class that runs the game loop.
 *
 */
class Application {
 public:
  /**
   * @brief Construct a new Application object.
   *
   */
  explicit Application(GraphicsAPI api = GraphicsAPI::OpenGL);

  /**
   * @brief Destroy the Application object.
   *
   */
  virtual ~Application();

  [[nodiscard]] GraphicsAPI GetGraphicsAPI() const { return graphics_api_; }

  virtual void Initialize();

  virtual void OnUpdate(float dt);

  /**
   * @brief Run the game loop.
   *
   */
  void Run();

  [[nodiscard]] GLFWwindow *GetWindow() const { return window_; }

  float GetDeltaTime();

  [[nodiscard]] int GetFPS() const { return fps_; }

  Ref<Scene> GetScene() { return scene_; }

  static Application *GetInstance();

  /// @brief Scene path parsed from the command line (`--scene <path>`); used by
  /// standalone apps (e.g. the sandbox) to load a scene at startup.
  static void SetStartupScenePath(const std::string &path) { startup_scene_path_ = path; }
  [[nodiscard]] static const std::string &GetStartupScenePath() { return startup_scene_path_; }

  /// @brief Graphics API parsed from the command line (`--api opengl|vulkan`);
  /// apps use this when choosing their backend.
  static void SetStartupApi(GraphicsAPI api) { startup_api_ = api; }
  [[nodiscard]] static GraphicsAPI GetStartupApi() { return startup_api_; }

  /// @brief Optional frame budget (`--frames <n>`): when positive the main loop
  /// exits after that many frames (headless/unattended smoke runs).
  static void SetMaxFrames(int frames) { max_frames_ = frames; }
  [[nodiscard]] static int GetMaxFrames() { return max_frames_; }

  /// @brief Whether the window should stay invisible (`--hidden`): rendering
  /// still happens into the default framebuffer, which is what unattended
  /// verification runs rely on.
  static void SetWindowHidden(bool hidden) { window_hidden_ = hidden; }
  [[nodiscard]] static bool IsWindowHidden() { return window_hidden_; }

 protected:
  /**
   * @brief scene_ is a unique pointer to the Scene class.
   *
   */
  Ref<Scene> scene_;

  Ref<FrameBuffer> frame_buffer_;

  int  viewport_width_{};
  int  viewport_height_{};
  bool viewport_resized_{};

  Entity selected_entity_;

  GLFWwindow *window_;

  float prev_time_;

  int   frame_count_;
  int   fps_;
  float frame_time_;

  Ref<ScriptEngine> script_engine_;

  Ref<IRHI> rhi_;

  GraphicsAPI graphics_api_;

  static std::string     startup_scene_path_;
  static GraphicsAPI     startup_api_;
  static int             max_frames_;
  static bool            window_hidden_;
};

}  // namespace MEngine

MEngine::Application *CreateApplication();
