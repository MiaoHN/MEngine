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

#include "scene/entity.hpp"

struct GLFWwindow;

namespace MEngine {

class FrameBuffer;
class ScriptEngine;
class Scene;
class Renderer;

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
  Application();

  /**
   * @brief Destroy the Application object.
   *
   */
  ~Application();

  virtual void Initialize();

  virtual void OnUpdate(float dt);

  /**
   * @brief Run the game loop.
   *
   */
  void Run();

  GLFWwindow *GetWindow() { return window_; }

  float GetDeltaTime();

  int GetFPS() { return fps_; }

  Ref<Scene> GetScene() { return scene_; }

  static Application *GetInstance();

 protected:
  /**
   * @brief scene_ is a unique pointer to the Scene class.
   *
   */
  Ref<Scene> scene_;

  Ref<FrameBuffer> frame_buffer_;

  int  viewport_width_;
  int  viewport_height_;
  bool viewport_resized_;

  Entity selected_entity_;

  GLFWwindow *window_;

  float prev_time_;

  int   frame_count_;
  int   fps_;
  float frame_time_;

  Ref<ScriptEngine> script_engine_;
};

}  // namespace MEngine

MEngine::Application *CreateApplication();
