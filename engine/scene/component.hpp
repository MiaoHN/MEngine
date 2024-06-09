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
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <string>

#include "render/gl.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"

namespace MEngine {

struct Tag {
  std::string tag;

  Tag(std::string tag) : tag(tag) {}
  Tag() = default;
};

struct Sprite2D {
  glm::vec3 position;
  glm::vec3 scale;
  glm::vec3 rotation;
  glm::vec4 color;

  std::shared_ptr<Texture> texture;

  Sprite2D(glm::vec3 position, glm::vec3 scale, glm::vec3 rotation,
         glm::vec4 color, std::shared_ptr<Texture> texture)
      : position(position),
        scale(scale),
        rotation(rotation),
        color(color),
        texture(texture) {}

  Sprite2D() = default;

  glm::mat4 GetModelMatrix() {
    glm::mat4 model = glm::mat4(1.0f);
    model           = glm::translate(model, position);
    model           = glm::rotate(model, glm::radians(rotation.x),
                                  glm::vec3(1.0f, 0.0f, 0.0f));
    model           = glm::rotate(model, glm::radians(rotation.y),
                                  glm::vec3(0.0f, 1.0f, 0.0f));
    model           = glm::rotate(model, glm::radians(rotation.z),
                                  glm::vec3(0.0f, 0.0f, 1.0f));
    model           = glm::scale(model, scale);
    return model;
  }
};

struct AABB {
  glm::vec3 position;
  glm::vec3 scale;

  AABB(glm::vec3 position, glm::vec3 scale)
      : position(position), scale(scale) {}

  AABB() = default;
};

struct Circle {
  glm::vec3 position;
  float radius;

  Circle(glm::vec3 position, float radius)
      : position(position), radius(radius) {}

  Circle() = default;
};

}  // namespace MEngine