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

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "render/light.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/model_loader.hpp"
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

  /// @brief Sets the orientation from an arbitrary axis-angle (angle in
  /// degrees), e.g. LearnOpenGL's `rotate(model, radians(angle), axis)`.
  /// Internally converts to the stored Euler degrees so the editor / animation
  /// / Lua / physics-writeback all keep working unchanged, while the net
  /// rotation exactly equals an axis-angle rotation about `axis`.
  void SetRotationAxisAngle(const glm::vec3 &axis, float degrees) {
    glm::vec3 a = axis;
    if (glm::length2(a) < 1e-6f) {
      a = glm::vec3(0.0f, 1.0f, 0.0f);
    } else {
      a = glm::normalize(a);
    }
    rotation = glm::degrees(glm::eulerAngles(glm::angleAxis(glm::radians(degrees), a)));
  }
};

/// @brief Parent link in the scene hierarchy. Entities WITHOUT this component
/// are root-level. An entity's `Transform` is relative to its parent's, so the
/// world transform of an entity is `parentWorld * localTransform` composed up
/// the chain. Parent/child relationships form a tree; cycles are rejected by
/// Scene::SetParent.
struct RelationshipComponent {
  entt::entity parent = entt::null;

  RelationshipComponent() = default;
  explicit RelationshipComponent(entt::entity parent) : parent(parent) {}
};

/// @brief One (time, value) sample on a transform animation channel. `time` is
/// in seconds on the scene's animation timeline (see Scene::SetAnimationTime).
struct Keyframe {
  float     time  = 0.0f;
  glm::vec3 value{0.0f};

  Keyframe() = default;
  Keyframe(float time, const glm::vec3 &value) : time(time), value(value) {}
};

/// @brief Keyframe transform animation for an entity. Each channel is a sorted
/// list of keyframes; while the scene timeline plays, non-empty channels drive
/// the corresponding `Transform` field (empty channels leave it untouched).
/// Rotation keyframes store degrees as XYZ Euler angles to match `Transform`.
/// Looping is a scene-wide timeline setting (Scene::SetAnimationLoop), not a
/// per-entity property, so every animated entity shares one clock.
struct AnimationComponent {
  std::vector<Keyframe> translation_keys;
  std::vector<Keyframe> rotation_keys;  // degrees, XYZ Euler
  std::vector<Keyframe> scale_keys;

  AnimationComponent() = default;

  /// @brief True when no channel carries any keyframe.
  [[nodiscard]] bool Empty() const {
    return translation_keys.empty() && rotation_keys.empty() && scale_keys.empty();
  }

  /// @brief Length of the longest channel (seconds). 0 when there are no keys.
  [[nodiscard]] float Duration() const {
    float duration = 0.0f;
    const auto last_time = [](const std::vector<Keyframe> &keys) {
      return keys.empty() ? 0.0f : keys.back().time;
    };
    duration = std::max(duration, last_time(translation_keys));
    duration = std::max(duration, last_time(rotation_keys));
    duration = std::max(duration, last_time(scale_keys));
    return duration;
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

/**
 * @brief Attaches a multi-material 3D model to a single entity.
 *
 * Requires a `Transform` component; every part of `model` is drawn under that
 * same entity transform (the model formalization of the old "root + one child
 * MeshComponent per material" import). Each part keeps its own mesh + material
 * (see `Model` / `ModelPart`); `source` is the model file path used to (re)load
 * it during scene serialization.
 */
struct ModelComponent {
  Ref<Model>  model;
  std::string source;  // model file path, for scene (de)serialization

  ModelComponent() = default;
  ModelComponent(Ref<Model> model, std::string source)
      : model(std::move(model)), source(std::move(source)) {}

  /// @brief Number of renderable parts (0 when there is no model).
  [[nodiscard]] size_t PartCount() const { return model ? model->parts.size() : 0; }
};

/// @brief Attaches a rigid body to an entity (requires a ColliderComponent).
/// @brief A point light carried by an entity (ECS). When at least one entity in
/// the scene has a PointLightComponent the scene drives the renderer's point
/// lights from these entities every frame - position comes from the entity's
/// world Transform (gizmo-movable) and colour/intensity/radius/shadow from this
/// component. The component's own `light.position` is ignored while the entity
/// also has a Transform. Scenes that only use the legacy `Scene::AddPointLight`
/// list API (no light components) are unaffected.
struct PointLightComponent {
  PointLight light;

  PointLightComponent() = default;
  explicit PointLightComponent(const PointLight &l) : light(l) {}
};

/// @brief Spot-light counterpart of PointLightComponent (position from the
/// entity Transform; direction/cone from `light`).
struct SpotLightComponent {
  SpotLight light;

  SpotLightComponent() = default;
  explicit SpotLightComponent(const SpotLight &l) : light(l) {}
};

/// @brief A directional light carried by an entity. When an entity has a
/// DirectionalLightComponent the scene copies it into the renderer's directional
/// light each frame - the FIRST such entity becomes the shadow-casting primary
/// sun and any further ones become additional (unshadowed) directional lights
/// (engine pbr/blinn shader arrays). Scenes without the component keep using the
/// legacy Scene::GetLight/SetLight authored sun.
struct DirectionalLightComponent {
  DirectionalLight light;

  DirectionalLightComponent() = default;
  explicit DirectionalLightComponent(const DirectionalLight &l) : light(l) {}
};

struct RigidBodyComponent {
  enum class Type { Static, Dynamic };

  Type  type        = Type::Dynamic;
  float friction    = 0.5f;
  float restitution = 0.0f;
  bool  continuous_collision = false;  // CCD (Jolt LinearCast) — prevents tunneling of fast bodies
  bool  is_sensor             = false;  // trigger: no collision response, contact events still fire

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