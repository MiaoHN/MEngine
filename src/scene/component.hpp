/**
 * @file component.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-21
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "render/gl.hpp"
#include "render/shader.hpp"

namespace MEngine {

struct RenderInfo {
  std::shared_ptr<Shader>          shader;
  std::shared_ptr<GL::VertexArray> vertex_array;

  RenderInfo(std::shared_ptr<Shader>          shader,
             std::shared_ptr<GL::VertexArray> vertex_array)
      : shader(shader), vertex_array(vertex_array) {}
  RenderInfo() = default;
};

struct Transform {
  glm::vec3 position;
  glm::vec3 rotation;
  glm::vec3 scale;

  Transform(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale)
      : position(position), rotation(rotation), scale(scale) {}
  Transform() = default;

  glm::mat4 GetModelMatrix() {
    glm::mat4 model = glm::mat4(1.0f);
    model           = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    model = glm::scale(model, scale);
    return model;
  }
};

}  // namespace MEngine