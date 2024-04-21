/**
 * @file component.hpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-04-21
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <memory>

#include "gl.hpp"
#include "shader.hpp"

namespace MEngine {

struct RenderInfo {
  std::shared_ptr<Shader>          shader;
  std::shared_ptr<GL::VertexArray> vertex_array;

  RenderInfo(std::shared_ptr<Shader>          shader,
             std::shared_ptr<GL::VertexArray> vertex_array)
      : shader(shader), vertex_array(vertex_array) {}
  RenderInfo() = default;
};

}  // namespace MEngine