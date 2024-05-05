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

struct GLFWwindow;

namespace MEngine {

class TaskDispatcher;
class TaskHandler;
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

  /**
   * @brief Run the game loop.
   *
   */
  void Run();

  GLFWwindow* GetWindow() { return window_; }

  float GetDeltaTime();

  static Application* GetInstance();

 private:
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

  GLFWwindow* window_;

  float prev_time_;

  std::shared_ptr<TaskHandler> renderer_;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine