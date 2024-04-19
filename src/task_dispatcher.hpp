/**
 * @file task_dispatcher.hpp
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
#include <vector>

namespace MEngine {

class Command;
class TaskHandler;

/**
 * @brief Dispatch tasks to different task handlers.
 * @todo Implement RemoveHandler method .etc.
 */
class TaskDispatcher {
 public:
  TaskDispatcher();
  ~TaskDispatcher();

  void PushHandler(std::shared_ptr<TaskHandler> task_handler);

  /**
   * @brief Run the command with the task handlers.
   * 
   * @param cmd The command to run.
   */
  void Run(Command* cmd);

 private:
  std::vector<std::shared_ptr<TaskHandler>> task_handlers_;
};

}  // namespace MEngine
