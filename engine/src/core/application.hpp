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

class AudioSystem;
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

  /// @brief The engine-wide audio subsystem (owned by this Application). Audio
  /// output opens lazily on first use; see AudioSystem::Initialize().
  [[nodiscard]] Ref<AudioSystem> GetAudio() const { return audio_; }

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

  /// @brief Startup window size. Defaults to 1600x900; LearnOpenGL-comparison
  /// examples set it to 800x600 (LO's exact window / 4:3 aspect) before their
  /// Application is constructed.
  static void SetStartupWindowSize(int width, int height) {
    startup_window_width_  = width;
    startup_window_height_ = height;
  }
  [[nodiscard]] static int GetStartupWindowWidth() { return startup_window_width_; }
  [[nodiscard]] static int GetStartupWindowHeight() { return startup_window_height_; }

  /// @brief Asset-root-relative equirectangular HDR used as the IBL/skybox
  /// environment. Defaults to the engine skybox; LO IBL demos (6.pbr 2.x) set it
  /// to newport_loft.hdr before constructing their Application.
  static void SetEnvironmentHdrPath(const std::string &path) { environment_hdr_path_ = path; }
  [[nodiscard]] static const std::string &GetEnvironmentHdrPath() { return environment_hdr_path_; }

  /// @brief Whether to vertically flip the equirectangular HDR on load (LO
  /// always flips; the engine's default kloppenheim env does not).
  static void SetEnvironmentHdrFlip(bool flip) { environment_hdr_flip_ = flip; }
  [[nodiscard]] static bool GetEnvironmentHdrFlip() { return environment_hdr_flip_; }

  /// @brief Captures the backbuffer as PPM after frame `frame` (`--capture-frame
  /// <n>`), writing to `out_path` (default "capture.ppm"). 0 disables capture.
  static void SetCaptureFrame(int frame, const std::string &out_path) {
    capture_frame_ = frame;
    capture_out_path_ = out_path;
  }
  [[nodiscard]] static int GetCaptureFrame() { return capture_frame_; }
  [[nodiscard]] static const std::string &GetCaptureOutPath() { return capture_out_path_; }

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

  Ref<AudioSystem> audio_;

  Ref<IRHI> rhi_;

  GraphicsAPI graphics_api_;

  /// @brief Base text of the window title. The live FPS suffix is appended
  /// automatically once a second by Application::UpdateWindowTitle; apps set
  /// their own name here (e.g. each example sets its demo label in ExampleApp).
  void SetWindowTitleBase(const std::string &title);
  /// @brief Rebuilds the GLFW title = `window_title_base_` + live FPS / ms.
  void UpdateWindowTitle();
  std::string window_title_base_ = "MEngine";

  static std::string     startup_scene_path_;
  static GraphicsAPI     startup_api_;
  static int             max_frames_;
  static bool            window_hidden_;
  static int             capture_frame_;
  static std::string     capture_out_path_;
  static int             startup_window_width_;
  static int             startup_window_height_;
  static std::string     environment_hdr_path_;
  static bool            environment_hdr_flip_;
};

}  // namespace MEngine

MEngine::Application *CreateApplication();
