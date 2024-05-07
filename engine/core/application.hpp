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

#include <memory>

#include "core/logger.hpp"
#include "scene/entity.hpp"

struct GLFWwindow;

namespace MEngine {

class FrameBuffer;
class TaskDispatcher;
class TaskHandler;
class ScriptEngine;
class Scene;

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

  GLFWwindow* GetWindow() { return window_; }

  float GetDeltaTime();

  int GetFPS() { return fps_; }

  std::shared_ptr<Scene> GetScene() { return scene_; }

  static Application* GetInstance();

 protected:
  /**
   * @brief task_dispatcher_ is a unique pointer to the TaskDispatcher class.
   *
   */
  std::unique_ptr<TaskDispatcher> task_dispatcher_;

  /**
   * @brief scene_ is a unique pointer to the Scene class.
   *
   */
  std::shared_ptr<Scene> scene_;

  std::shared_ptr<FrameBuffer> frame_buffer_;

  int  viewport_width_;
  int  viewport_height_;
  bool viewport_resized_;

  Entity selected_entity_;

  GLFWwindow* window_;

  float prev_time_;

  int   frame_count_;
  int   fps_;
  float frame_time_;

  std::shared_ptr<TaskHandler> renderer_;

  std::shared_ptr<ScriptEngine> script_engine_;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine

MEngine::Application* CreateApplication();
