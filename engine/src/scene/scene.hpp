/**
 * @file scene.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-16
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "core/common.hpp"
#include "core/logger.hpp"
#include "core/script_engine.hpp"
#include "physics/physics_world.hpp"
#include "render/light.hpp"
#include "scene/camera.hpp"
#include "scene/component.hpp"
#include "scene/entity.hpp"

namespace MEngine {

class Renderer;
enum class RenderMode;

class Scene {
 public:
  Scene();
  ~Scene();

  Entity CreateEntity(const std::string &name = "Unnamed Entity") {
    Entity entity = Entity(registry_.create(), &registry_);
    entity.AddComponent<Tag>(name);
    entities_.push_back(entity);
    LOG_DEBUG("Scene") << "Created entity '" << name << "'";
    return entity;
  }

  void DestroyEntity(Entity entity) {
    const std::string name = entity.GetComponent<Tag>().tag;
    registry_.destroy(entity.GetHandle());

    for (auto it = entities_.begin(); it != entities_.end(); ++it) {
      if (*it == entity) {
        entities_.erase(it);
        break;
      }
    }
    LOG_DEBUG("Scene") << "Destroyed entity '" << name << "'";
  }

  template <typename... Components>
  auto GetAllEntitiesWith() {
    auto                view = registry_.view<Components...>();
    std::vector<Entity> entities;
    for (auto entity : view) {
      entities.push_back(Entity(entity, &registry_));
    }
    return entities;
  }

  std::vector<Entity> &GetAllEntities() { return entities_; }

  void LoadScene(const std::string &path);
  void SaveScene(const std::string &path);

  Ref<Camera> GetDefaultCameraInfo() { return default_camera_info_; }

  void OnUpdateEditor(const Camera &camera);

  void OnUpdateSimulation(float dt, const Camera &camera);

  void OnUpdateRuntime(float dt, int vw, int vh);

  void Render(const Camera &camera);

  /// @brief Draw all entities with a MeshComponent using the given camera.
  /// `target_fbo` selects the framebuffer the final composite is drawn into
  /// (0 = default framebuffer); `target_width`/`target_height` override the
  /// composite viewport when rendering into a custom framebuffer.
  void RenderMeshes(const glm::mat4 &view, const glm::mat4 &proj, const glm::vec3 &camera_pos,
                    unsigned int target_fbo = 0, int target_width = 0, int target_height = 0);

  /// @brief Renders the 3D scene from the primary camera (falling back to the
  /// default camera when none is marked primary) into `target_fbo`. Used by
  /// the editor's Play mode.
  void RenderFromPrimaryCamera(unsigned int target_fbo = 0, int target_width = 0, int target_height = 0);

  /// @brief Returns true when at least one entity has a primary camera.
  [[nodiscard]] bool HasPrimaryCamera();

  void AddPointLight(const PointLight &light);
  void ClearPointLights();

  void AddSpotLight(const SpotLight &light);
  void ClearSpotLights();

  [[nodiscard]] const DirectionalLight &GetLight() const;
  DirectionalLight &GetLight();
  void SetLight(const DirectionalLight &light);

  void SetExposure(float exposure);
  void SetBloomStrength(float strength);
  void SetBloomThreshold(float threshold);
  void SetShadowPcfRadius(float radius);
  void SetIblIntensity(float intensity);
  void SetGodRaysStrength(float strength);
  void SetSSAOEnabled(bool enabled);
  void SetTAAEnabled(bool enabled);
  void SetBloomEnabled(bool enabled);

  [[nodiscard]] bool       IsSSAOEnabled() const;
  [[nodiscard]] bool       IsTAAEnabled() const;
  [[nodiscard]] bool       IsBloomEnabled() const;
  [[nodiscard]] float      GetExposure() const;
  [[nodiscard]] float      GetBloomStrength() const;
  [[nodiscard]] float      GetBloomThreshold() const;
  [[nodiscard]] float      GetShadowPcfRadius() const;
  [[nodiscard]] float      GetIblIntensity() const;
  [[nodiscard]] float      GetGodRaysStrength() const;

  void SetRenderMode(RenderMode mode);
  [[nodiscard]] RenderMode GetRenderMode() const;

  /// @brief Builds Jolt bodies from RigidBody/Collider components and
  /// snapshots their transforms so StopSimulation can restore them.
  void StartSimulation();

  /// @brief Steps the physics world and writes body transforms back to the
  /// matching Transform components.
  void StepSimulation(float delta_time);

  /// @brief Destroys all physics bodies and restores the initial transforms.
  void StopSimulation();

  [[nodiscard]] bool IsSimulating() const { return simulating_; }

  [[nodiscard]] PhysicsWorld &GetPhysicsWorld() { return *physics_world_; }

  /// @brief Moves/rotates every entity with a CameraController + CameraComponent
  /// using WASD/QE keys and the given mouse delta. `look_active` enables
  /// right-drag look; the pitch is clamped to avoid flipping. Call during Play
  /// mode only.
  void UpdateCameraControllers(float delta_time, const glm::vec2 &mouse_delta, bool look_active);

  /// @brief The scene's Lua scripting engine (per-entity scripts + main script).
  [[nodiscard]] ScriptEngine &GetScriptEngine() { return *script_engine_; }

  /// @brief Optional scene-level Lua main script path (e.g. "scripts/main.lua").
  void SetMainScript(const std::string &path) { main_script_ = path; }
  [[nodiscard]] const std::string &GetMainScript() const { return main_script_; }

  /// @brief Finds an entity by tag name (returns a null entity when absent).
  [[nodiscard]] Entity FindEntityByName(const std::string &name);

  /// @brief Raw registry access (used by the Lua bindings).
  [[nodiscard]] entt::registry &GetRegistry() { return registry_; }

  // --- Physics helpers for the Lua bindings --------------------------------
  // All are no-ops unless the scene is simulating and the entity owns a body.

  /// @brief Creates a Jolt body for `handle` if it is eligible (Transform +
  /// Collider + RigidBody) and does not have one yet; destroys its body when
  /// it is no longer eligible. Public so scripts can add components and spawn
  /// colliding entities while a simulation is running.
  void RefreshEntityBody(entt::entity handle);

  /// @brief True while the entity has a live physics body.
  [[nodiscard]] bool HasPhysicsBody(entt::entity handle);

  /// @brief Casts a ray against the physics world and returns the handle of the
  /// entity owning the closest hit body (entt::null when nothing is hit).
  /// `out_distance` (optional) receives the hit distance from `origin`.
  [[nodiscard]] entt::entity Raycast(const glm::vec3 &origin, const glm::vec3 &direction, float max_distance,
                                     float *out_distance = nullptr) const;

  /// @brief Linear velocity of the entity's body (zero when it has none).
  glm::vec3 GetBodyVelocity(entt::entity handle);

  /// @brief Sets the linear velocity of a dynamic body (wakes it up).
  void SetBodyVelocity(entt::entity handle, const glm::vec3 &velocity);

  /// @brief Applies an impulse at the body's center of mass (wakes it up).
  void ApplyBodyImpulse(entt::entity handle, const glm::vec3 &impulse);

 private:
  entt::registry registry_;

  std::vector<Entity> entities_;

  Ref<Camera> default_camera_info_;

  Ref<Renderer> renderer_;

  Ref<PhysicsWorld> physics_world_;
  bool              simulating_ = false;

  /// @brief Accumulator for the fixed-step simulation (physics + OnFixedUpdate
  /// + collision dispatch all advance at kFixedTimeStep).
  float sim_accumulator_ = 0.0f;

  std::unordered_map<entt::entity, JPH::BodyID> body_ids_;
  std::unordered_map<uint32_t, entt::entity>    body_id_to_entity_;

  /// @brief JSON snapshot of the authoring (content, non-editor) entities,
  /// taken when a simulation starts. StopSimulation restores the scene from it
  /// so script-side changes (moves, colors, spawned/destroyed entities) never
  /// leak back into Edit mode.
  std::string play_snapshot_;

  /// @brief Serializes the current content entities into play_snapshot_.
  void CapturePlaySnapshot();

  /// @brief Replaces every content entity with the captured snapshot, leaving
  /// editor-only entities (e.g. the grid) untouched.
  void RestorePlaySnapshot();

  /// @brief Synchronizes Jolt bodies with the RigidBody/Collider components
  /// (used to pick up entities spawned or re-configured at runtime).
  void SyncSimulationBodies();

  /// @brief Writes simulated body transforms back to the Transform components.
  void WriteBackTransforms();

  /// @brief Drains contact events and forwards them to the script engine.
  void DispatchContactEvents();

  Ref<ScriptEngine> script_engine_;
  std::string       main_script_;
};

}  // namespace MEngine