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
#include <string>
#include <utility>
#include <vector>

#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/texture.hpp"
#include "scene/camera.hpp"

namespace MEngine {

struct Tag {
  std::string tag;
  bool        editor_only = false;

  Tag(std::string tag) : tag(tag) {}
  Tag() = default;
};

struct CameraComponent {
  Camera camera;
  bool   primary = false;

  CameraComponent() = default;
  explicit CameraComponent(const Camera &camera) : camera(camera) {}
};

/// @brief Makes a camera entity user-controllable during Play mode (free-fly).
/// Attach alongside `CameraComponent`; WASD/QE move the camera and holding the
/// right mouse button + dragging looks around.
struct CameraController {
  float move_speed       = 5.0f;    // world units per second
  float look_sensitivity = 0.15f;   // degrees per pixel of mouse movement

  CameraController() = default;
};

struct Transform {
  glm::vec3 translation = {0.0f, 0.0f, 0.0f};
  glm::vec3 rotation    = {0.0f, 0.0f, 0.0f};
  glm::vec3 scale       = {1.0f, 1.0f, 1.0f};

  Transform()                  = default;
  Transform(const Transform &) = default;
  Transform &operator=(const Transform &) = default;
  Transform(const glm::vec3 &translation) : translation(translation) {}

  glm::mat4 GetTransform() const {
    // NOTE: rename the local to avoid shadowing the `rotation` member; the
    // quaternion is built from the member Euler angles. `rotation` is stored
    // in degrees (consistent with Sprite2D), so convert to radians here.
    glm::mat4 rotation_matrix = glm::toMat4(glm::quat(glm::radians(rotation)));

    return glm::translate(glm::mat4(1.0f), translation) * rotation_matrix * glm::scale(glm::mat4(1.0f), scale);
  }
};

/**
 * @brief Attaches a renderable 3D mesh to an entity.
 *
 * Requires a `Transform` component to position the mesh; if absent the mesh is
 * drawn with an identity model matrix.
 */
struct MeshComponent {
  Ref<Mesh>     mesh;
  Ref<Material> material;

  MeshComponent() = default;
  MeshComponent(Ref<Mesh> mesh, Ref<Material> material)
      : mesh(std::move(mesh)), material(std::move(material)) {}
};

/// @brief Attaches a rigid body to an entity (requires a ColliderComponent).
struct RigidBodyComponent {
  enum class Type { Static, Dynamic };

  Type  type        = Type::Dynamic;
  float friction    = 0.5f;
  float restitution = 0.0f;

  RigidBodyComponent() = default;
  explicit RigidBodyComponent(Type type) : type(type) {}
};

/// @brief One collision shape attached to an entity. Shapes are world-space
/// (the transform's scale is intentionally ignored). A ColliderComponent can
/// hold several shapes; the physics world builds a compound body out of them.
struct ColliderComponent {
  enum class Shape { Box, Sphere, Capsule, Cylinder };

  Shape     shape                = Shape::Box;
  glm::vec3 box_half_extents{0.5f, 0.5f, 0.5f};
  float     sphere_radius        = 0.5f;
  float     capsule_radius       = 0.5f;
  float     capsule_half_height  = 0.5f;  // half of the cylindrical middle segment
  float     cylinder_radius      = 0.5f;
  float     cylinder_half_height = 0.5f;
  glm::vec3 offset{0.0f, 0.0f, 0.0f};

  ColliderComponent() = default;

  /// @brief True when the shape list is a single shape whose data lives on the
  /// component (used by serialization / the Lua single-shape API).
  [[nodiscard]] float ShapeRadius() const {
    switch (shape) {
      case Shape::Sphere: return sphere_radius;
      case Shape::Capsule: return capsule_radius;
      case Shape::Cylinder: return cylinder_radius;
      default: return 0.0f;
    }
  }
};

/// @brief One shape description (used by the collider-group API).
struct ColliderShapeData {
  enum class Shape { Box, Sphere, Capsule, Cylinder };

  Shape     shape                = Shape::Box;
  glm::vec3 box_half_extents{0.5f, 0.5f, 0.5f};
  float     sphere_radius        = 0.5f;
  float     capsule_radius       = 0.5f;
  float     capsule_half_height  = 0.5f;
  float     cylinder_radius      = 0.5f;
  float     cylinder_half_height = 0.5f;
  glm::vec3 offset{0.0f, 0.0f, 0.0f};  // local offset inside the compound body
};

/// @brief Optional extra collision shapes on an entity. When present they are
/// merged with the entity's `ColliderComponent` (if any) into one Jolt
/// compound body, so a single entity can own several collision boxes/shapes.
struct ColliderGroupComponent {
  std::vector<ColliderShapeData> shapes;

  ColliderGroupComponent() = default;
  explicit ColliderGroupComponent(std::vector<ColliderShapeData> shapes) : shapes(std::move(shapes)) {}

  [[nodiscard]] bool Empty() const { return shapes.empty(); }
};

/// @brief Attaches a Lua script to an entity. The script runs with `self` =
/// this entity and may define OnStart/OnUpdate/OnFixedUpdate/OnDestroy hooks.
struct LuaScriptComponent {
  std::string path;

  LuaScriptComponent() = default;
  explicit LuaScriptComponent(std::string path) : path(std::move(path)) {}
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