#include "core/script_engine.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_map>

#include "core/input.hpp"
#include "render/asset_manager.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "scene/component.hpp"
#include "scene/scene.hpp"

namespace MEngine {

namespace {

const char *kEntityMeta = "MEngine.Entity";

struct EntityUserdata {
  Scene       *scene = nullptr;
  entt::entity id    = entt::null;
};

Scene *GetLuaScene(lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "MEngine.Scene");
  auto *scene = static_cast<Scene *>(lua_touserdata(L, -1));
  lua_pop(L, 1);
  return scene;
}

ScriptEngine *GetLuaEngine(lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, "MEngine.Engine");
  auto *engine = static_cast<ScriptEngine *>(lua_touserdata(L, -1));
  lua_pop(L, 1);
  return engine;
}

EntityUserdata *CheckEntity(lua_State *L, int index) {
  return static_cast<EntityUserdata *>(luaL_checkudata(L, index, kEntityMeta));
}

void PushEntity(lua_State *L, Scene *scene, entt::entity id) {
  auto *ud  = static_cast<EntityUserdata *>(lua_newuserdata(L, sizeof(EntityUserdata)));
  ud->scene = scene;
  ud->id    = id;
  luaL_setmetatable(L, kEntityMeta);
}

/// @brief Sets `table.field = { x, y, z }` (table at the top of the stack).
void PushVec3Field(lua_State *L, const char *field, const glm::vec3 &v) {
  lua_newtable(L);
  lua_pushnumber(L, v.x);
  lua_rawseti(L, -2, 1);
  lua_pushnumber(L, v.y);
  lua_rawseti(L, -2, 2);
  lua_pushnumber(L, v.z);
  lua_rawseti(L, -2, 3);
  lua_setfield(L, -2, field);
}

int TracebackHandler(lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  luaL_traceback(L, L, msg, 1);
  return 1;
}

int ProtectedCall(lua_State *L, int nargs, int nresults) {
  const int base = lua_gettop(L) - nargs;  // function index
  lua_pushcfunction(L, TracebackHandler);
  lua_insert(L, base);
  const int status = lua_pcall(L, nargs, nresults, base);
  lua_remove(L, base);
  return status;
}

/// @brief Maps a user-friendly key name to a GLFW key code (or -1 if unknown).
int KeyNameToGlfw(const std::string &name) {
  if (name.size() == 1) {
    const char c = name[0];
    if (c >= 'A' && c <= 'Z') return GLFW_KEY_A + (c - 'A');
    if (c >= 'a' && c <= 'z') return GLFW_KEY_A + (c - 'a');
    if (c >= '0' && c <= '9') return GLFW_KEY_0 + (c - '0');
  }
  static const std::unordered_map<std::string, int> kKeys = {
      {"Space", GLFW_KEY_SPACE},           {"Escape", GLFW_KEY_ESCAPE},
      {"Enter", GLFW_KEY_ENTER},           {"Tab", GLFW_KEY_TAB},
      {"Backspace", GLFW_KEY_BACKSPACE},   {"Left", GLFW_KEY_LEFT},
      {"Right", GLFW_KEY_RIGHT},           {"Up", GLFW_KEY_UP},
      {"Down", GLFW_KEY_DOWN},             {"LeftShift", GLFW_KEY_LEFT_SHIFT},
      {"RightShift", GLFW_KEY_RIGHT_SHIFT}, {"LeftControl", GLFW_KEY_LEFT_CONTROL},
      {"RightControl", GLFW_KEY_RIGHT_CONTROL}, {"LeftAlt", GLFW_KEY_LEFT_ALT},
      {"RightAlt", GLFW_KEY_RIGHT_ALT},
  };
  const auto it = kKeys.find(name);
  return it != kKeys.end() ? it->second : -1;
}

// --- MEngine global API ----------------------------------------------------

int Api_Log(lua_State *L) {
  LOG_INFO("Lua") << luaL_checkstring(L, 1);
  return 0;
}

int Api_Time(lua_State *L) {
  lua_pushnumber(L, GetLuaEngine(L)->GetElapsedTime());
  return 1;
}

int Api_DeltaTime(lua_State *L) {
  lua_pushnumber(L, GetLuaEngine(L)->GetDeltaTime());
  return 1;
}

int Api_IsKeyDown(lua_State *L) {
  const int key = KeyNameToGlfw(luaL_checkstring(L, 1));
  lua_pushboolean(L, key >= 0 && Input::IsKeyPressed(key));
  return 1;
}

int Api_FindEntity(lua_State *L) {
  Scene      *scene = GetLuaScene(L);
  const char *name  = luaL_checkstring(L, 1);
  for (auto &entity : scene->GetAllEntities()) {
    if (!entity.HasComponent<Tag>() || entity.GetComponent<Tag>().editor_only) continue;
    if (entity.GetComponent<Tag>().tag == name) {
      PushEntity(L, scene, entity.GetHandle());
      return 1;
    }
  }
  lua_pushnil(L);
  return 1;
}

int Api_CreateEntity(lua_State *L) {
  Scene      *scene  = GetLuaScene(L);
  const char *name   = luaL_optstring(L, 1, "Entity");
  Entity      entity = scene->CreateEntity(name);
  PushEntity(L, scene, entity.GetHandle());
  return 1;
}

int Api_DestroyEntity(lua_State *L) {
  Scene          *scene = GetLuaScene(L);
  EntityUserdata *ud    = CheckEntity(L, 1);
  scene->DestroyEntity(Entity(ud->id, &scene->GetRegistry()));
  return 0;
}

int Api_GetEntities(lua_State *L) {
  Scene *scene = GetLuaScene(L);
  std::vector<entt::entity> ids;
  for (auto &entity : scene->GetAllEntities()) {
    if (entity.HasComponent<Tag>() && entity.GetComponent<Tag>().editor_only) continue;
    ids.push_back(entity.GetHandle());
  }
  lua_newtable(L);
  for (size_t i = 0; i < ids.size(); ++i) {
    PushEntity(L, scene, ids[i]);
    lua_rawseti(L, -2, static_cast<int>(i + 1));
  }
  return 1;
}

// --- entity object methods -------------------------------------------------

int Entity_GetName(lua_State *L) {
  EntityUserdata *ud  = CheckEntity(L, 1);
  auto           *tag = ud->scene->GetRegistry().try_get<Tag>(ud->id);
  lua_pushstring(L, tag ? tag->tag.c_str() : "");
  return 1;
}

int Entity_SetName(lua_State *L) {
  EntityUserdata *ud   = CheckEntity(L, 1);
  const char     *name = luaL_checkstring(L, 2);
  auto           *tag  = ud->scene->GetRegistry().try_get<Tag>(ud->id);
  if (tag) tag->tag = name;
  return 0;
}

int Entity_GetPosition(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  auto           *t  = ud->scene->GetRegistry().try_get<Transform>(ud->id);
  lua_pushnumber(L, t ? t->translation.x : 0.0);
  lua_pushnumber(L, t ? t->translation.y : 0.0);
  lua_pushnumber(L, t ? t->translation.z : 0.0);
  return 3;
}

int Entity_SetPosition(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  const glm::vec3 p(static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                    static_cast<float>(luaL_checknumber(L, 4)));
  auto *t = ud->scene->GetRegistry().try_get<Transform>(ud->id);
  if (t) t->translation = p;
  return 0;
}

int Entity_GetRotation(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  auto           *t  = ud->scene->GetRegistry().try_get<Transform>(ud->id);
  lua_pushnumber(L, t ? t->rotation.x : 0.0);
  lua_pushnumber(L, t ? t->rotation.y : 0.0);
  lua_pushnumber(L, t ? t->rotation.z : 0.0);
  return 3;
}

int Entity_SetRotation(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  const glm::vec3 r(static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                    static_cast<float>(luaL_checknumber(L, 4)));
  auto *t = ud->scene->GetRegistry().try_get<Transform>(ud->id);
  if (t) t->rotation = r;
  return 0;
}

int Entity_GetScale(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  auto           *t  = ud->scene->GetRegistry().try_get<Transform>(ud->id);
  lua_pushnumber(L, t ? t->scale.x : 1.0);
  lua_pushnumber(L, t ? t->scale.y : 1.0);
  lua_pushnumber(L, t ? t->scale.z : 1.0);
  return 3;
}

int Entity_SetScale(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  const glm::vec3 s(static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                    static_cast<float>(luaL_checknumber(L, 4)));
  auto *t = ud->scene->GetRegistry().try_get<Transform>(ud->id);
  if (t) t->scale = s;
  return 0;
}

int Entity_GetId(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  lua_pushinteger(L, static_cast<lua_Integer>(entt::to_integral(ud->id)));
  return 1;
}

// --- entity component & physics API ----------------------------------------

/// @brief Creates a primitive mesh from a Lua shape name ("cube"/"plane"/"sphere").
Ref<Mesh> PrimitiveMesh(const std::string &shape) {
  if (shape == "sphere") return Mesh::CreateSphere();
  if (shape == "plane") return Mesh::CreatePlane();
  return Mesh::CreateCube();
}

/// @brief Default white, matte PBR material for script-created meshes.
Ref<Material> DefaultScriptMaterial() {
  auto material = CreateRef<Material>();
  if (auto shader = AssetManager::Instance().GetShader("pbr")) {
    material->SetShader(shader);
  }
  material->SetBaseColorFactor(glm::vec4(1.0f));
  material->SetMetallicFactor(0.0f);
  material->SetRoughnessFactor(0.85f);
  material->SetSpecularFactor(0.3f);
  return material;
}

int Entity_HasComponent(lua_State *L) {
  EntityUserdata    *ud   = CheckEntity(L, 1);
  const std::string  name = luaL_checkstring(L, 2);
  auto              &reg  = ud->scene->GetRegistry();
  bool               has  = false;
  if (name == "transform") has = reg.all_of<Transform>(ud->id);
  else if (name == "mesh") has = reg.all_of<MeshComponent>(ud->id);
  else if (name == "rigid_body") has = reg.all_of<RigidBodyComponent>(ud->id);
  else if (name == "collider") has = reg.all_of<ColliderComponent>(ud->id);
  else if (name == "collider_group") has = reg.all_of<ColliderGroupComponent>(ud->id);
  else if (name == "camera") has = reg.all_of<CameraComponent>(ud->id);
  else if (name == "lua_script") has = reg.all_of<LuaScriptComponent>(ud->id);
  lua_pushboolean(L, has);
  return 1;
}

int Entity_AddComponent(lua_State *L) {
  EntityUserdata    *ud  = CheckEntity(L, 1);
  const std::string  name = luaL_checkstring(L, 2);
  auto              &reg = ud->scene->GetRegistry();
  if (!reg.valid(ud->id)) {
    lua_pushboolean(L, false);
    return 1;
  }
  const bool simulating = ud->scene->IsSimulating();

  if (name == "transform") {
    if (!reg.all_of<Transform>(ud->id)) reg.emplace<Transform>(ud->id);
    if (simulating) ud->scene->RefreshEntityBody(ud->id);
  } else if (name == "mesh") {
    const std::string shape = luaL_optstring(L, 3, "cube");
    Ref<Mesh>        mesh   = PrimitiveMesh(shape);
    Ref<Material>    mat    = DefaultScriptMaterial();
    if (!reg.all_of<MeshComponent>(ud->id)) {
      reg.emplace<MeshComponent>(ud->id, mesh, mat);
    } else {
      auto &c = reg.get<MeshComponent>(ud->id);
      c.mesh = mesh;
      c.material = mat;
    }
  } else if (name == "collider") {
    const std::string shape = luaL_optstring(L, 3, "box");
    ColliderComponent collider;
    if (shape == "sphere") {
      collider.shape         = ColliderComponent::Shape::Sphere;
      collider.sphere_radius = static_cast<float>(luaL_optnumber(L, 4, 0.5));
    } else if (shape == "capsule") {
      collider.shape               = ColliderComponent::Shape::Capsule;
      collider.capsule_radius      = static_cast<float>(luaL_optnumber(L, 4, 0.5));
      collider.capsule_half_height = static_cast<float>(luaL_optnumber(L, 5, 0.5));
    } else if (shape == "cylinder") {
      collider.shape                = ColliderComponent::Shape::Cylinder;
      collider.cylinder_radius      = static_cast<float>(luaL_optnumber(L, 4, 0.5));
      collider.cylinder_half_height = static_cast<float>(luaL_optnumber(L, 5, 0.5));
    }
    if (!reg.all_of<ColliderComponent>(ud->id)) {
      reg.emplace<ColliderComponent>(ud->id, collider);
    } else {
      reg.get<ColliderComponent>(ud->id) = collider;
    }
    if (simulating) ud->scene->RefreshEntityBody(ud->id);
  } else if (name == "collider_group") {
    // Appends one extra shape to the entity's compound collider group.
    const std::string shape = luaL_optstring(L, 3, "box");
    ColliderShapeData s;
    if (shape == "sphere") {
      s.shape = ColliderShapeData::Shape::Sphere;
      s.sphere_radius = static_cast<float>(luaL_optnumber(L, 4, 0.5));
    } else if (shape == "capsule") {
      s.shape = ColliderShapeData::Shape::Capsule;
      s.capsule_radius = static_cast<float>(luaL_optnumber(L, 4, 0.5));
      s.capsule_half_height = static_cast<float>(luaL_optnumber(L, 5, 0.5));
    } else if (shape == "cylinder") {
      s.shape = ColliderShapeData::Shape::Cylinder;
      s.cylinder_radius = static_cast<float>(luaL_optnumber(L, 4, 0.5));
      s.cylinder_half_height = static_cast<float>(luaL_optnumber(L, 5, 0.5));
    }
    if (!reg.all_of<ColliderGroupComponent>(ud->id)) {
      reg.emplace<ColliderGroupComponent>(ud->id, std::vector<ColliderShapeData>{s});
    } else {
      reg.get<ColliderGroupComponent>(ud->id).shapes.push_back(s);
    }
    if (simulating) ud->scene->RefreshEntityBody(ud->id);
  } else if (name == "rigid_body") {
    const std::string   type = luaL_optstring(L, 3, "dynamic");
    RigidBodyComponent rb;
    rb.type        = (type == "static") ? RigidBodyComponent::Type::Static : RigidBodyComponent::Type::Dynamic;
    rb.friction    = static_cast<float>(luaL_optnumber(L, 4, 0.5));
    rb.restitution = static_cast<float>(luaL_optnumber(L, 5, 0.0));    rb.continuous_collision = lua_toboolean(L, 6);
    rb.is_sensor             = lua_toboolean(L, 7);    if (!reg.all_of<RigidBodyComponent>(ud->id)) {
      reg.emplace<RigidBodyComponent>(ud->id, rb);
    } else {
      reg.get<RigidBodyComponent>(ud->id) = rb;
    }
    if (simulating) ud->scene->RefreshEntityBody(ud->id);
  } else if (name == "camera") {
    if (!reg.all_of<CameraComponent>(ud->id)) reg.emplace<CameraComponent>(ud->id);
  } else if (name == "lua_script") {
    const char *path = luaL_checkstring(L, 3);
    if (!reg.all_of<LuaScriptComponent>(ud->id)) {
      reg.emplace<LuaScriptComponent>(ud->id, std::string(path));
    } else {
      reg.get<LuaScriptComponent>(ud->id).path = path;
    }
  } else {
    LOG_WARN("Lua") << "add_component: unknown component '" << name << "'";
  }

  PushEntity(L, ud->scene, ud->id);
  return 1;
}

int Entity_RemoveComponent(lua_State *L) {
  EntityUserdata    *ud   = CheckEntity(L, 1);
  const std::string  name = luaL_checkstring(L, 2);
  auto              &reg  = ud->scene->GetRegistry();
  if (!reg.valid(ud->id)) {
    lua_pushboolean(L, false);
    return 1;
  }
  const bool simulating = ud->scene->IsSimulating();

  if (name == "transform" && reg.all_of<Transform>(ud->id)) reg.remove<Transform>(ud->id);
  else if (name == "mesh" && reg.all_of<MeshComponent>(ud->id)) reg.remove<MeshComponent>(ud->id);
  else if (name == "collider" && reg.all_of<ColliderComponent>(ud->id)) reg.remove<ColliderComponent>(ud->id);
  else if (name == "collider_group" && reg.all_of<ColliderGroupComponent>(ud->id))
    reg.remove<ColliderGroupComponent>(ud->id);
  else if (name == "rigid_body" && reg.all_of<RigidBodyComponent>(ud->id)) reg.remove<RigidBodyComponent>(ud->id);
  else if (name == "camera" && reg.all_of<CameraComponent>(ud->id)) reg.remove<CameraComponent>(ud->id);
  else if (name == "lua_script" && reg.all_of<LuaScriptComponent>(ud->id)) reg.remove<LuaScriptComponent>(ud->id);

  if (simulating && (name == "transform" || name == "collider" || name == "collider_group" ||
                     name == "rigid_body")) {
    ud->scene->RefreshEntityBody(ud->id);
  }

  PushEntity(L, ud->scene, ud->id);
  return 1;
}

int Entity_GetVelocity(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  const glm::vec3 v  = ud->scene->GetBodyVelocity(ud->id);
  lua_pushnumber(L, v.x);
  lua_pushnumber(L, v.y);
  lua_pushnumber(L, v.z);
  return 3;
}

int Entity_SetVelocity(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  const glm::vec3 v(static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                    static_cast<float>(luaL_checknumber(L, 4)));
  ud->scene->SetBodyVelocity(ud->id, v);
  return 0;
}

int Entity_ApplyImpulse(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  const glm::vec3 i(static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                    static_cast<float>(luaL_checknumber(L, 4)));
  ud->scene->ApplyBodyImpulse(ud->id, i);
  return 0;
}

int Entity_GetColor(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  glm::vec4       c(1.0f);
  if (auto *mc = ud->scene->GetRegistry().try_get<MeshComponent>(ud->id)) {
    if (mc->material) c = mc->material->GetBaseColorFactor();
  }
  lua_pushnumber(L, c.r);
  lua_pushnumber(L, c.g);
  lua_pushnumber(L, c.b);
  lua_pushnumber(L, c.a);
  return 4;
}

int Entity_SetColor(lua_State *L) {
  EntityUserdata *ud = CheckEntity(L, 1);
  const glm::vec4 c(static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                    static_cast<float>(luaL_checknumber(L, 4)),
                    static_cast<float>(luaL_optnumber(L, 5, 1.0)));
  if (auto *mc = ud->scene->GetRegistry().try_get<MeshComponent>(ud->id)) {
    if (mc->material) mc->material->SetBaseColorFactor(c);
  }
  return 0;
}

}  // namespace

LuaScriptInstance::LuaScriptInstance(lua_State *L, Scene *scene, entt::entity entity, std::string path)
    : L_(L), scene_(scene), entity_(entity), path_(std::move(path)) {}

LuaScriptInstance::~LuaScriptInstance() {
  if (L_ && env_ref_ != LUA_NOREF) {
    luaL_unref(L_, LUA_REGISTRYINDEX, env_ref_);
    env_ref_ = LUA_NOREF;
  }
}

bool LuaScriptInstance::Load() {
  // Create a per-script environment table whose metatable falls back to the
  // global table, then expose the entity as `self`.
  lua_newtable(L_);  // env
  lua_newtable(L_);  // mt
  lua_getglobal(L_, "_G");
  lua_setfield(L_, -2, "__index");
  lua_setmetatable(L_, -2);

  if (entity_ != entt::null) {
    PushEntity(L_, scene_, entity_);
  } else {
    lua_pushnil(L_);
  }
  lua_setfield(L_, -2, "self");
  env_ref_ = luaL_ref(L_, LUA_REGISTRYINDEX);

  const std::string resolved = AssetManager::Instance().Resolve(path_);
  if (luaL_loadfile(L_, resolved.c_str()) != LUA_OK) {
    LOG_ERROR("Lua") << "Failed to load '" << resolved << "': " << lua_tostring(L_, -1);
    lua_pop(L_, 1);
    return false;
  }

  // Point the chunk's _ENV upvalue at our environment table.
  lua_rawgeti(L_, LUA_REGISTRYINDEX, env_ref_);
  if (lua_setupvalue(L_, -2, 1) == nullptr) {
    lua_pop(L_, 1);
  }

  if (ProtectedCall(L_, 0, 0) != LUA_OK) {
    LOG_ERROR("Lua") << "Error running '" << path_ << "': " << lua_tostring(L_, -1);
    lua_pop(L_, 1);
    return false;
  }

  valid_ = true;
  return true;
}

bool LuaScriptInstance::CallHook(const char *name, int nargs) {
  if (!valid_) return false;

  lua_rawgeti(L_, LUA_REGISTRYINDEX, env_ref_);  // [args..., env]
  lua_getfield(L_, -1, name);                    // [args..., env, hook]
  if (!lua_isfunction(L_, -1)) {
    lua_pop(L_, nargs + 2);
    return false;
  }

  lua_remove(L_, -2);          // [args..., hook]
  lua_insert(L_, -nargs - 1);  // [hook, args...]
  const int status = ProtectedCall(L_, nargs, 0);
  if (status != LUA_OK) {
    LOG_ERROR("Lua") << "Error in '" << path_ << "." << name << "': " << lua_tostring(L_, -1);
    lua_pop(L_, 1);
  }
  return status == LUA_OK;
}

void LuaScriptInstance::CallStart() {
  if (started_ || !valid_) return;
  started_ = true;
  CallHook("OnStart", 0);
}

void LuaScriptInstance::CallUpdate(float dt) {
  if (!valid_) return;
  lua_pushnumber(L_, dt);
  CallHook("OnUpdate", 1);
}

void LuaScriptInstance::CallFixedUpdate(float dt) {
  if (!valid_) return;
  lua_pushnumber(L_, dt);
  CallHook("OnFixedUpdate", 1);
}

void LuaScriptInstance::CallDestroy() {
  if (!valid_) return;
  CallHook("OnDestroy", 0);
}

void LuaScriptInstance::CallCollisionEnter(entt::entity other, const ScriptCollisionInfo &info) {
  if (!valid_) return;
  PushEntity(L_, scene_, other);
  // collision = { point = {x,y,z}, normal = {x,y,z}, relative_velocity = {x,y,z}, penetration = n }
  lua_newtable(L_);
  PushVec3Field(L_, "point", info.point);
  PushVec3Field(L_, "normal", info.normal);
  PushVec3Field(L_, "relative_velocity", info.relative_velocity);
  lua_pushnumber(L_, info.penetration);
  lua_setfield(L_, -2, "penetration");
  CallHook("OnCollisionEnter", 2);
}

void LuaScriptInstance::CallCollisionExit(entt::entity other) {
  if (!valid_) return;
  PushEntity(L_, scene_, other);
  CallHook("OnCollisionExit", 1);
}

ScriptEngine::ScriptEngine(Scene *scene) : scene_(scene) {
  L_ = luaL_newstate();
  luaL_openlibs(L_);

  // Stash the scene/engine pointers so C callbacks can reach them.
  lua_pushlightuserdata(L_, scene_);
  lua_setfield(L_, LUA_REGISTRYINDEX, "MEngine.Scene");
  lua_pushlightuserdata(L_, this);
  lua_setfield(L_, LUA_REGISTRYINDEX, "MEngine.Engine");

  RegisterApi();
}

ScriptEngine::~ScriptEngine() {
  Clear();
  if (L_) {
    lua_close(L_);
    L_ = nullptr;
  }
}

void ScriptEngine::RegisterApi() {
  // Entity metatable.
  luaL_newmetatable(L_, kEntityMeta);
  lua_pushvalue(L_, -1);
  lua_setfield(L_, -2, "__index");
  lua_pushcfunction(L_, Entity_GetName);
  lua_setfield(L_, -2, "get_name");
  lua_pushcfunction(L_, Entity_SetName);
  lua_setfield(L_, -2, "set_name");
  lua_pushcfunction(L_, Entity_GetPosition);
  lua_setfield(L_, -2, "get_position");
  lua_pushcfunction(L_, Entity_SetPosition);
  lua_setfield(L_, -2, "set_position");
  lua_pushcfunction(L_, Entity_GetRotation);
  lua_setfield(L_, -2, "get_rotation");
  lua_pushcfunction(L_, Entity_SetRotation);
  lua_setfield(L_, -2, "set_rotation");
  lua_pushcfunction(L_, Entity_GetScale);
  lua_setfield(L_, -2, "get_scale");
  lua_pushcfunction(L_, Entity_SetScale);
  lua_setfield(L_, -2, "set_scale");
  lua_pushcfunction(L_, Entity_GetId);
  lua_setfield(L_, -2, "get_id");
  lua_pushcfunction(L_, Entity_HasComponent);
  lua_setfield(L_, -2, "has_component");
  lua_pushcfunction(L_, Entity_AddComponent);
  lua_setfield(L_, -2, "add_component");
  lua_pushcfunction(L_, Entity_RemoveComponent);
  lua_setfield(L_, -2, "remove_component");
  lua_pushcfunction(L_, Entity_GetVelocity);
  lua_setfield(L_, -2, "get_velocity");
  lua_pushcfunction(L_, Entity_SetVelocity);
  lua_setfield(L_, -2, "set_velocity");
  lua_pushcfunction(L_, Entity_ApplyImpulse);
  lua_setfield(L_, -2, "apply_impulse");
  lua_pushcfunction(L_, Entity_GetColor);
  lua_setfield(L_, -2, "get_color");
  lua_pushcfunction(L_, Entity_SetColor);
  lua_setfield(L_, -2, "set_color");
  lua_pop(L_, 1);

  // Global `MEngine` table.
  lua_newtable(L_);
  lua_pushcfunction(L_, Api_Log);
  lua_setfield(L_, -2, "log");
  lua_pushcfunction(L_, Api_Time);
  lua_setfield(L_, -2, "time");
  lua_pushcfunction(L_, Api_DeltaTime);
  lua_setfield(L_, -2, "delta_time");
  lua_pushcfunction(L_, Api_IsKeyDown);
  lua_setfield(L_, -2, "is_key_down");
  lua_pushcfunction(L_, Api_FindEntity);
  lua_setfield(L_, -2, "find_entity");
  lua_pushcfunction(L_, Api_CreateEntity);
  lua_setfield(L_, -2, "create_entity");
  lua_pushcfunction(L_, Api_DestroyEntity);
  lua_setfield(L_, -2, "destroy_entity");
  lua_pushcfunction(L_, Api_GetEntities);
  lua_setfield(L_, -2, "get_entities");
  lua_setglobal(L_, "MEngine");
}

void ScriptEngine::SyncInstances() {
  // Snapshot the current script components (entity id -> path).
  std::unordered_map<entt::entity, std::string> desired;
  for (auto &entity : scene_->GetAllEntities()) {
    if (entity.HasComponent<LuaScriptComponent>()) {
      desired[entity.GetHandle()] = entity.GetComponent<LuaScriptComponent>().path;
    }
  }

  // Drop instances whose entity no longer has a script component.
  for (auto it = instances_.begin(); it != instances_.end();) {
    if (desired.count((*it)->GetEntity()) == 0) {
      (*it)->CallDestroy();
      it = instances_.erase(it);
    } else {
      ++it;
    }
  }

  // Create instances for new script components (skip paths that already
  // failed to load, so a missing file doesn't spam the log every frame).
  for (auto &entry : desired) {
    const entt::entity id   = entry.first;
    const std::string &path = entry.second;
    if (failed_paths_.count(path) != 0) continue;

    const auto it = std::find_if(instances_.begin(), instances_.end(),
                                 [id](const auto &inst) { return inst->GetEntity() == id; });
    if (it == instances_.end()) {
      auto inst = std::make_unique<LuaScriptInstance>(L_, scene_, id, path);
      if (inst->Load()) {
        instances_.push_back(std::move(inst));
      } else {
        failed_paths_.insert(path);
      }
    }
  }
}

void ScriptEngine::Update(float dt) {
  delta_time_   = dt;
  elapsed_time_ += dt;

  SyncInstances();

  if (main_script_) {
    main_script_->CallStart();
    main_script_->CallUpdate(dt);
  }
  for (auto &inst : instances_) {
    inst->CallStart();
    inst->CallUpdate(dt);
  }
}

void ScriptEngine::FixedStepUpdate(float dt) {
  // dt is always kFixedTimeStep (driven by the Scene's fixed-step accumulator).
  for (auto &inst : instances_) {
    if (inst->IsStarted()) inst->CallFixedUpdate(dt);
  }
  if (main_script_ && main_script_->IsStarted()) {
    main_script_->CallFixedUpdate(dt);
  }
}

void ScriptEngine::StartAll() {
  SyncInstances();
  if (main_script_) main_script_->CallStart();
  for (auto &inst : instances_) {
    inst->CallStart();
  }
}

void ScriptEngine::LoadMainScript(const std::string &path) {
  main_script_path_ = path;
  main_script_.reset();
  if (path.empty()) return;

  auto inst = std::make_unique<LuaScriptInstance>(L_, scene_, entt::null, path);
  if (inst->Load()) {
    main_script_ = std::move(inst);
  }
}

void ScriptEngine::ReloadScript(const std::string &path) {
  for (auto it = instances_.begin(); it != instances_.end();) {
    if ((*it)->GetPath() == path) {
      (*it)->CallDestroy();
      it = instances_.erase(it);
    } else {
      ++it;
    }
  }
  failed_paths_.erase(path);

  if (path == main_script_path_) {
    LoadMainScript(path);
  }
}

void ScriptEngine::Clear() {
  for (auto &inst : instances_) {
    inst->CallDestroy();
  }
  instances_.clear();
  if (main_script_) {
    main_script_->CallDestroy();
    main_script_.reset();
  }
  failed_paths_.clear();
}

void ScriptEngine::DispatchCollision(entt::entity entity, entt::entity other, const ScriptCollisionInfo &info,
                                     bool enter) {
  for (auto &inst : instances_) {
    if (inst->GetEntity() != entity || !inst->IsStarted()) continue;
    if (enter) {
      inst->CallCollisionEnter(other, info);
    } else {
      inst->CallCollisionExit(other);
    }
  }
}

}  // namespace MEngine
