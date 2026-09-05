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
#include "render/renderer.hpp"
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

  /// @brief Destroys an entity and, recursively, its whole child subtree
  /// (children first, so no dangling RelationshipComponent is left behind).
  void DestroyEntity(Entity entity);

  /// @brief Parents `child` under `parent`. Passing `entt::null` detaches the
  /// child to the root. Rejects making an entity a child of itself or of one
  /// of its own descendants (would form a cycle). Returns false when invalid.
  bool SetParent(entt::entity child, entt::entity parent);

  /// @brief Parent handle of `entity` (`entt::null` when root-level).
  [[nodiscard]] entt::entity GetParent(entt::entity entity) const;

  /// @brief True when `entity` has at least one direct child.
  [[nodiscard]] bool HasChildren(entt::entity entity) const;

  /// @brief Direct children of `entity`, in creation order (empty when none).
  [[nodiscard]] std::vector<entt::entity> GetChildren(entt::entity entity) const;

  /// @brief True when `entity` is `ancestor` or lies somewhere below it.
  [[nodiscard]] bool IsDescendantOf(entt::entity entity, entt::entity ancestor) const;

  /// @brief World (hierarchy-composed) transform of `entity`. Equals the
  /// entity's local Transform when it is root-level or has no Transform.
  [[nodiscard]] glm::mat4 GetWorldTransform(entt::entity entity) const;

  /// @brief World-space position of the entity's Transform (origin when the
  /// entity has no Transform).
  [[nodiscard]] glm::vec3 GetWorldPosition(entt::entity entity) const;

  /// @brief Rewrites the entity's local TRS so that it lands at the given
  /// world transform (used by the editor gizmo on children and by reparenting
  /// that must keep a world pose stable). No-op without a Transform.
  void SetLocalTransformFromWorld(entt::entity entity, const glm::mat4 &world);

  // --- Keyframe animation timeline (scene-wide) -----------------------------
  // The scene has one shared clock. `SetAnimationTime` scrubs/previews a pose
  // by sampling every entity's AnimationComponent into its local Transform.
  // Play mode auto-plays any animated scene from t = 0 (see StartSimulation).

  /// @brief Sets the shared timeline cursor to `time` seconds (clamped to the
  /// scene's animation duration) and applies the sampled pose to every
  /// animated entity. This is how the editor previews / scrubs poses.
  void SetAnimationTime(float time);

  /// @brief Current scene animation time in seconds.
  [[nodiscard]] float GetAnimationTime() const { return anim_time_; }

  /// @brief Longest animation duration across all animated entities (0 when
  /// the scene has no animation).
  [[nodiscard]] float GetAnimationDuration() const;

  /// @brief True when at least one entity has a non-empty AnimationComponent.
  [[nodiscard]] bool HasAnyAnimation() const;

  /// @brief Advances the shared clock by `dt` and applies the new pose.
  /// Respects the loop setting (wraps) or stops at the end when not looping.
  void AdvanceAnimation(float dt);

  /// @brief Starts/stops Edit-mode timeline playback. Play mode automatically
  /// plays any animated scene regardless of this flag.
  void SetAnimationPlaying(bool playing) { anim_playing_ = playing; }
  [[nodiscard]] bool IsAnimationPlaying() const { return anim_playing_; }

  /// @brief Whether the clock wraps (true) or clamps-and-stops at the end.
  void SetAnimationLoop(bool loop) { anim_loop_ = loop; }
  [[nodiscard]] bool GetAnimationLoop() const { return anim_loop_; }

  /// @brief Rewinds the clock to 0 and applies the t = 0 pose.
  void ResetAnimation();

  /// @brief Timeline / clip length in seconds. The playhead range and loop wrap
  /// use this (it is independent of where the last keyframe happens to be, so
  /// the playhead stays movable before any key exists). Grows are optional;
  /// keys beyond this length are clamped out of playback.
  void SetAnimationLength(float seconds);
  [[nodiscard]] float GetAnimationLength() const { return anim_length_; }

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

  /// @brief Starts a brand-new scene: stops any running simulation, clears the
  /// content entities (editor-only helpers such as the grid are kept), resets
  /// the script engine and the main-script path.
  void ClearContent();

  /// @brief Replaces the scene content with the entities / settings of a
  /// `.scene` file, keeping editor-only helpers intact. Returns false when the
  /// file could not be read or parsed. Does not start scripts (the caller runs
  /// them on Play).
  bool OpenSceneFile(const std::string &path);

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

  /// @brief Fills `out` with the per-pass timings of the last frame
  /// (shadow / point shadows / ssao / main / skybox / post), in milliseconds.
  void GetLastPassTimes(float out_times[6]) const;

  /// @brief Per-frame render counters of the last rendered frame.
  [[nodiscard]] const RenderStats &GetRenderStats() const;

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

  /// @brief Shared keyframe-animation timeline clock and playback state.
  float anim_time_    = 0.0f;
  float anim_length_  = 1.0f;  // timeline/clip length in seconds (scrub range)
  bool  anim_playing_ = false;
  bool  anim_loop_    = true;

  /// @brief Samples every entity's AnimationComponent at anim_time_ into its
  /// local Transform.
  void ApplyAnimations();

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

  /// @brief Appends `handle` and every descendant to `out` in post-order
  /// (children before their parent).
  void CollectSubtree(entt::entity handle, std::vector<entt::entity> &out) const;

  /// @brief Stops the simulation (destroys bodies) if running.
  void StopSimulationIfRunning();

  /// @brief Destroys every content (non-editor-only) entity, keeping
  /// editor-only helpers (e.g. the grid).
  void RemoveContentEntities();

  /// @brief Synchronizes Jolt bodies with the RigidBody/Collider components
  /// (used to pick up entities spawned or re-configured at runtime).
  void SyncSimulationBodies();

  /// @brief Writes simulated body transforms back to the Transform components.
  void WriteBackTransforms();

  /// @brief Drains contact events and forwards them to the script engine.
  void DispatchContactEvents();

  Ref<ScriptEngine> script_engine_;
  std::string       main_script_;

  /// @brief Per-pass timings of the last rendered frame (ms).
  float pass_times_ms_[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  /// @brief Rendered-frame counter for the periodic render-stats log.
  int stats_log_frames_ = 0;
};

}  // namespace MEngine