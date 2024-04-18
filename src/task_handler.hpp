/**
 * @file task_handler.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-18
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <string>

namespace MEngine {

class Command;

class TaskHandler {
 public:
  TaskHandler();

  virtual ~TaskHandler() = default;

  virtual void Run(Command& cmd);
};

}  // namespace MEngine
