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

#include "core/logger.hpp"
#include "core/task_handler.hpp"
#include "render/gl.hpp"

namespace MEngine {

class Command;

class Renderer : public TaskHandler {
 public:
  Renderer();
  ~Renderer();

  void Run(Command* command) override;

 private:
  std::shared_ptr<GL::VertexArray> vertex_array_;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine
