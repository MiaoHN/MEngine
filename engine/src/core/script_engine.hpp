/**
 * @file script_engine.hpp
 * @brief Lua scripting: per-entity script components and a scene main script.
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <entt/entt.hpp>

#include "core/common.hpp"
#include "core/logger.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace MEngine {

class Scene;

/// @brief The simulation's fixed physics/script step (seconds).
inline constexpr float kFixedTimeStep = 1.0f / 60.0f;

/// @brief Rich data delivered to `OnCollisionEnter(other, collision)`.
/// All vectors are oriented for the receiving script:
///   - `normal` points from `other` toward the receiving entity (`self`);
///   - `relative_velocity` is `velocity(other) - velocity(self)`.
struct ScriptCollisionInfo {
  glm::vec3 point{0.0f};
  glm::vec3 normal{0.0f};
  glm::vec3 relative_velocity{0.0f};
  float     penetration = 0.0f;
};

/// @brief One running Lua script bound to an entity (or the scene as a main
/// script). The script runs in its own environment table with `self` = entity.
class LuaScriptInstance {
 public:
  LuaScriptInstance(lua_State *L, Scene *scene, entt::entity entity, std::string path);
  ~LuaScriptInstance();

  bool Load();

  void CallStart();
  void CallUpdate(float dt);
  void CallFixedUpdate(float dt);
  void CallDestroy();
  void CallCollisionEnter(entt::entity other, const ScriptCollisionInfo &info);
  void CallCollisionExit(entt::entity other);

  [[nodiscard]] entt::entity GetEntity() const { return entity_; }
  [[nodiscard]] bool          IsStarted() const { return started_; }
  [[nodiscard]] const std::string &GetPath() const { return path_; }

 private:
  bool CallHook(const char *name, int nargs);

  lua_State   *L_      = nullptr;
  Scene       *scene_  = nullptr;
  entt::entity entity_ = entt::null;
  std::string  path_;
  int          env_ref_ = LUA_NOREF;
  bool         started_ = false;
  bool         valid_   = false;
};

/// @brief Owns the Lua state and all running script instances for a Scene.
class ScriptEngine {
 public:
  explicit ScriptEngine(Scene *scene);
  ~ScriptEngine();

  ScriptEngine(const ScriptEngine &)            = delete;
  ScriptEngine &operator=(const ScriptEngine &) = delete;

  /// @brief Per-frame update: syncs script components with running instances,
  /// then calls OnStart / OnUpdate.
  void Update(float dt);

  /// @brief Runs one fixed step's `OnFixedUpdate` callbacks. Called by the
  /// Scene's fixed-step accumulator (after each physics step).
  void FixedStepUpdate(float dt);

  /// @brief Syncs instances and calls OnStart on every attached script (and
  /// the main script) without running OnUpdate. Call once after building the
  /// physics bodies so collision events can be delivered to started scripts on
  /// the very first physics step.
  void StartAll();

  /// @brief Loads the scene-level main script.
  void LoadMainScript(const std::string &path);

  /// @brief Reloads a script file (drops its instances so they re-run OnStart).
  void ReloadScript(const std::string &path);

  /// @brief Destroys all instances (calls OnDestroy) and clears state.
  void Clear();

  /// @brief Calls OnCollisionEnter (with `info`) / OnCollisionExit on the
  /// script bound to `entity`, passing `other` as the other entity.
  void DispatchCollision(entt::entity entity, entt::entity other, const ScriptCollisionInfo &info, bool enter);

  [[nodiscard]] float      GetElapsedTime() const { return elapsed_time_; }
  [[nodiscard]] float      GetDeltaTime() const { return delta_time_; }
  [[nodiscard]] lua_State *GetLuaState() { return L_; }

 private:
  void RegisterApi();
  void SyncInstances();

  lua_State *L_     = nullptr;
  Scene     *scene_ = nullptr;

  float elapsed_time_ = 0.0f;
  float delta_time_   = 0.0f;

  std::vector<std::unique_ptr<LuaScriptInstance>> instances_;
  std::unique_ptr<LuaScriptInstance>              main_script_;
  std::string                                     main_script_path_;
  std::unordered_set<std::string>                 failed_paths_;
};

}  // namespace MEngine
