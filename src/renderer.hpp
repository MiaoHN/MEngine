/**
 * @file renderer.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-19
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <memory>

#include "logger.hpp"
#include "task_handler.hpp"

namespace MEngine {

class Command;

class Renderer : public TaskHandler {
 public:
  Renderer();
  ~Renderer();

  void Run(Command* command) override;

 private:
  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine
