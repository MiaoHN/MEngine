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
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
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

struct Camera2D {
  glm::vec3 position;
  float     rotation;

  float aspect_ratio;
  float zoom_level;

  bool primary = false;

  glm::mat4 view;
  glm::mat4 projection;

  Camera2D(float left, float right, float bottom, float top, float zoom_level, bool primary)
      : position(glm::vec3((left + right) / 2, (bottom + top) / 2, 0.0f)),
        rotation(0.0f),
        aspect_ratio((right - left) / (top - bottom)),
        zoom_level(zoom_level),
        primary(primary) {}

  Camera2D(glm::vec3 position, float rotation, float aspect_ratio, float zoom_level, bool primary)
      : position(position), rotation(rotation), aspect_ratio(aspect_ratio), zoom_level(zoom_level), primary(primary) {}

  Camera2D() = default;

  void SetProjection(float left, float right, float bottom, float top) {
    projection = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
  }

  void OnWindowResize(float width, float height) {
    aspect_ratio = width / height;
    SetProjection(-aspect_ratio * zoom_level, aspect_ratio * zoom_level, -zoom_level, zoom_level);
  }

  void OnMouseScroll(float y_offset) {
    zoom_level += y_offset;
    if (zoom_level < 0.25f) zoom_level = 0.25f;
    SetProjection(-aspect_ratio * zoom_level, aspect_ratio * zoom_level, -zoom_level, zoom_level);
  }

  void SetPosition(const glm::vec3 &pos) { position = pos; }

  void SetRotation(float rotation) { rotation = rotation; }

  const glm::mat4 &GetViewMatrix() const { return view; }

  const glm::mat4 &GetProjectionMatrix() const { return projection; }

  glm::mat4 GetProjectionView() {
    RecalculateViewMatrix();
    return projection * view;
  }

  const glm::vec3 &GetPosition() const { return position; }

  glm::vec3 &GetPosition() { return position; }

  const float &GetRotation() const { return rotation; }

  float &GetRotation() { return rotation; }

  void RecalculateViewMatrix() {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) *
                          glm::rotate(glm::mat4(1.0f), glm::radians(rotation), glm::vec3(0, 0, 1));

    view = glm::inverse(transform);
  }

  const float &GetZoomLevel() const { return zoom_level; }
  float       &GetZoomLevel() { return zoom_level; }
  const float &GetAspectRatio() const { return aspect_ratio; }
  float       &GetAspectRatio() { return aspect_ratio; }

  void SetZoomLevel(float zoom_level) {
    zoom_level = zoom_level;
    SetProjection(-aspect_ratio * zoom_level, aspect_ratio * zoom_level, -zoom_level, zoom_level);
  }
  void SetAspectRatio(float aspect_ratio) {
    aspect_ratio = aspect_ratio;
    SetProjection(-aspect_ratio * zoom_level, aspect_ratio * zoom_level, -zoom_level, zoom_level);
  }
};

struct Transform {
  glm::vec3 translation = {0.0f, 0.0f, 0.0f};
  glm::vec3 rotation    = {0.0f, 0.0f, 0.0f};
  glm::vec3 scale       = {1.0f, 1.0f, 1.0f};

  Transform()                  = default;
  Transform(const Transform &) = default;
  Transform(const glm::vec3 &translation) : translation(translation) {}

  glm::mat4 GetTransform() const {
    glm::mat4 rotation = glm::toMat4(glm::quat(rotation));

    return glm::translate(glm::mat4(1.0f), translation) * rotation * glm::scale(glm::mat4(1.0f), scale);
  }
};

struct Sprite2D {
  glm::vec3 position;
  glm::vec3 scale;
  glm::vec3 rotation;
  glm::vec4 color;

  float tiling_factor = 1.0f;

  Ref<Texture> texture;

  Sprite2D(glm::vec3 position, glm::vec3 scale, glm::vec3 rotation, glm::vec4 color, Ref<Texture> texture)
      : position(position), scale(scale), rotation(rotation), color(color), texture(texture) {}

  Sprite2D() = default;

  glm::mat4 GetModelMatrix() {
    glm::mat4 model = glm::mat4(1.0f);
    model           = glm::translate(model, position);
    model           = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model           = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model           = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model           = glm::scale(model, scale);
    return model;
  }
};

struct AnimatedSprite2D {
  glm::vec3 position;
  glm::vec3 scale;
  glm::vec3 rotation;
  glm::vec4 color;

  Ref<Texture> texture;

  int h_frames;
  int v_frames;

  float frame_time;
  float current_time;
  int   current_frame;

  AnimatedSprite2D(glm::vec3 position, glm::vec3 scale, glm::vec3 rotation, glm::vec4 color,
                   Ref<Texture> texture, int h_frames, int v_frames, float frame_time)
      : position(position),
        scale(scale),
        rotation(rotation),
        color(color),
        texture(texture),
        h_frames(h_frames),
        v_frames(v_frames),
        frame_time(frame_time),
        current_time(0.0f),
        current_frame(0) {}

  AnimatedSprite2D() = default;

  glm::mat4 GetModelMatrix() {
    glm::mat4 model = glm::mat4(1.0f);
    model           = glm::translate(model, position);
    model           = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model           = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model           = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model           = glm::scale(model, scale);
    return model;
  }
};

struct AABB {
  glm::vec3 position;
  glm::vec3 scale;

  AABB(glm::vec3 position, glm::vec3 scale) : position(position), scale(scale) {}

  AABB() = default;
};

struct Circle {
  glm::vec3 position;
  float     radius;

  Circle(glm::vec3 position, float radius) : position(position), radius(radius) {}

  Circle() = default;
};

}  // namespace MEngine