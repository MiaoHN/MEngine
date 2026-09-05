/**
 * @file scene_serializer.cpp
 * @brief JSON (de)serialization of a Scene for standalone play / persistence.
 *
 * Serializes the scene's entities (Tag / Transform / MeshComponent /
 * CameraComponent / RigidBodyComponent / ColliderComponent), lights and render
 * settings. Meshes are stored by source ("cube" / "plane" / "sphere" or a
 * model file path relative to the asset root); materials are stored as PBR
 * factors + texture paths relative to the asset root.
 */

#include "scene/scene.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include <json.hpp>

#include "core/logger.hpp"
#include "render/asset_manager.hpp"
#include "render/model_loader.hpp"
#include "render/renderer.hpp"
#include "scene/component.hpp"

namespace MEngine {

namespace {

using json = nlohmann::json;

// --- glm <-> json helpers -------------------------------------------------

json Vec3ToJson(const glm::vec3 &v) { return json::array({v.x, v.y, v.z}); }

glm::vec3 Vec3FromJson(const json &j, const glm::vec3 &fallback = glm::vec3(0.0f)) {
  if (!j.is_array() || j.size() < 3) return fallback;
  return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

json Vec4ToJson(const glm::vec4 &v) { return json::array({v.x, v.y, v.z, v.w}); }

glm::vec4 Vec4FromJson(const json &j, const glm::vec4 &fallback = glm::vec4(1.0f)) {
  if (!j.is_array() || j.size() < 4) return fallback;
  return glm::vec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
}

// --- asset path helpers ---------------------------------------------------

/// @brief Converts an absolute asset path to one relative to the asset root,
/// falling back to the original path when it is outside the asset root.
std::string ToAssetRelative(const std::string &path) {
  if (path.empty()) return path;

  const std::filesystem::path abs_root = std::filesystem::absolute(AssetManager::Instance().GetAssetRoot());
  std::error_code             ec;
  const std::filesystem::path rel = std::filesystem::relative(path, abs_root, ec);
  return ec ? path : rel.generic_string();
}

/// @brief Resolves a path relative to the asset root into a loadable path.
std::string ResolveAsset(const std::string &relative) { return AssetManager::Instance().Resolve(relative); }

// --- render mode -----------------------------------------------------------

std::string RenderModeToString(RenderMode mode) {
  switch (mode) {
    case RenderMode::Unlit:
      return "unlit";
    case RenderMode::Wireframe:
      return "wireframe";
    case RenderMode::Lit:
    default:
      return "lit";
  }
}

RenderMode RenderModeFromString(const std::string &str) {
  if (str == "unlit") return RenderMode::Unlit;
  if (str == "wireframe") return RenderMode::Wireframe;
  return RenderMode::Lit;
}

// --- mesh ------------------------------------------------------------------

/// @brief Rebuilds a mesh from its serialized source string.
/// @brief Rebuilds a mesh from its serialized source string. Shared meshes
/// (AssetManager cache) let same-source entities batch into instanced draws.
Ref<Mesh> MeshFromSource(const std::string &source) {
  Ref<Mesh> mesh = AssetManager::Instance().GetMesh(source);
  if (!mesh && !source.empty()) {
    LOG_WARN("SceneSerializer") << "Failed to load mesh from '" << source << "'";
  }
  return mesh;
}
// --- material --------------------------------------------------------------

json MaterialToJson(const Ref<Material> &material) {
  json j;
  if (!material) return j;

  j["base_color"] = Vec4ToJson(material->GetBaseColorFactor());
  j["metallic"]   = material->GetMetallicFactor();
  j["roughness"]  = material->GetRoughnessFactor();
  j["specular"]   = material->GetSpecularFactor();

  switch (material->GetCullMode()) {
    case CullMode::Back:
      j["cull"] = "back";
      break;
    case CullMode::Front:
      j["cull"] = "front";
      break;
    case CullMode::None:
    default:
      break;  // default omitted for backward-compatible scenes
  }

  const auto texture_path = [](const Ref<Texture> &texture) -> json {
    if (texture && !texture->GetPath().empty()) return ToAssetRelative(texture->GetPath());
    return nullptr;
  };
  j["albedo"]             = texture_path(material->GetAlbedoMap());
  j["normal"]             = texture_path(material->GetNormalMap());
  j["metallic_roughness"] = texture_path(material->GetMetallicRoughnessMap());
  j["ao"]                 = texture_path(material->GetAOMap());
  return j;
}

Ref<Material> MaterialFromJson(const json &j) {
  auto material = CreateRef<Material>();
  material->SetShader(AssetManager::Instance().GetShader("pbr"));

  material->SetBaseColorFactor(Vec4FromJson(j.value("base_color", json()), glm::vec4(1.0f)));
  material->SetMetallicFactor(j.value("metallic", 1.0f));
  material->SetRoughnessFactor(j.value("roughness", 1.0f));
  material->SetSpecularFactor(j.value("specular", 1.0f));

  const std::string cull = j.value("cull", "");
  if (cull == "back") {
    material->SetCullMode(CullMode::Back);
  } else if (cull == "front") {
    material->SetCullMode(CullMode::Front);
  }  // anything else stays None (backward compatible)

  const auto load_texture = [&j](const char *key) -> Ref<Texture> {
    if (j.contains(key) && j[key].is_string() && !j[key].get<std::string>().empty()) {
      return Texture::Create(ResolveAsset(j[key].get<std::string>()));
    }
    return nullptr;
  };
  material->SetAlbedoMap(load_texture("albedo"));
  material->SetNormalMap(load_texture("normal"));
  material->SetMetallicRoughnessMap(load_texture("metallic_roughness"));
  material->SetAOMap(load_texture("ao"));
  return material;
}

// --- camera ----------------------------------------------------------------

json CameraToJson(const Camera &camera) {
  json j;
  j["projection"] = camera.projection_type == ProjectionType::Perspective ? "perspective" : "orthographic";
  j["fov"]        = camera.fov_degrees;
  j["ortho_size"] = camera.ortho_size;
  j["near"]       = camera.near_plane;
  j["far"]        = camera.far_plane;
  j["position"]   = Vec3ToJson(camera.position);
  j["rotation"]   = Vec3ToJson(camera.rotation);
  return j;
}

void CameraFromJson(const json &j, Camera &camera) {
  camera.projection_type = (j.value("projection", "perspective") == "orthographic") ? ProjectionType::Orthographic
                                                                                     : ProjectionType::Perspective;
  camera.fov_degrees     = j.value("fov", 45.0f);
  camera.ortho_size      = j.value("ortho_size", 5.0f);
  camera.near_plane      = j.value("near", 0.1f);
  camera.far_plane       = j.value("far", 100.0f);
  camera.position        = Vec3FromJson(j.value("position", json()));
  camera.rotation        = Vec3FromJson(j.value("rotation", json()));
}

/// @brief Content (non-editor-only) entities in scene-creation order. The order
/// is stable across Save / snapshot / Load, so a document can reference an
/// entity by its index in the saved `entities` array.
std::vector<Entity> ContentEntities(Scene &scene) {
  std::vector<Entity> content;
  for (auto &entity : scene.GetAllEntities()) {
    const auto *tag = entity.HasComponent<Tag>() ? &entity.GetComponent<Tag>() : nullptr;
    if (tag && tag->editor_only) continue;
    content.push_back(entity);
  }
  return content;
}

/// @brief Serializes a single content entity to its JSON form. Used both by
/// SaveScene and by the Play-mode snapshot. `parent_index` is the index of the
/// entity's parent inside the same `entities` array (-1 = root-level, omitted).
json EntityToJson(Entity &entity, int parent_index = -1) {
  const auto &tag = entity.GetComponent<Tag>();

  json e;
  e["tag"] = tag.tag;
  if (parent_index >= 0) {
    e["parent"] = parent_index;
  }

  if (entity.HasComponent<Transform>()) {
    const auto &t = entity.GetComponent<Transform>();
    json        j;
    j["translation"] = Vec3ToJson(t.translation);
    j["rotation"]    = Vec3ToJson(t.rotation);
    j["scale"]       = Vec3ToJson(t.scale);
    e["transform"]   = j;
  }

  if (entity.HasComponent<MeshComponent>()) {
    const auto &mesh = entity.GetComponent<MeshComponent>();
    json        j;
    j["source"]   = mesh.mesh ? mesh.mesh->GetSource() : "";
    j["material"] = MaterialToJson(mesh.material);
    e["mesh"]     = j;
  }

  if (entity.HasComponent<CameraComponent>()) {
    const auto &component = entity.GetComponent<CameraComponent>();
    json        j         = CameraToJson(component.camera);
    j["primary"]          = component.primary;
    e["camera"]           = j;
  }

  if (entity.HasComponent<RigidBodyComponent>()) {
    const auto &component = entity.GetComponent<RigidBodyComponent>();
    json        j;
    j["type"]        = component.type == RigidBodyComponent::Type::Static ? "static" : "dynamic";
    j["friction"]    = component.friction;
    j["restitution"] = component.restitution;
    j["continuous_collision"] = component.continuous_collision;
    j["sensor"]                = component.is_sensor;
    e["rigid_body"]  = j;
  }

  if (entity.HasComponent<ColliderComponent>()) {
    const auto &component = entity.GetComponent<ColliderComponent>();
    json        j;
    switch (component.shape) {
      case ColliderComponent::Shape::Sphere:
        j["shape"] = "sphere";
        break;
      case ColliderComponent::Shape::Capsule:
        j["shape"] = "capsule";
        break;
      case ColliderComponent::Shape::Cylinder:
        j["shape"] = "cylinder";
        break;
      case ColliderComponent::Shape::Box:
      default:
        j["shape"] = "box";
        break;
    }
    j["half_extents"] = Vec3ToJson(component.box_half_extents);
    j["radius"]       = component.ShapeRadius();
    if (component.shape == ColliderComponent::Shape::Capsule) {
      j["half_height"] = component.capsule_half_height;
    } else if (component.shape == ColliderComponent::Shape::Cylinder) {
      j["half_height"] = component.cylinder_half_height;
    }
    j["offset"]   = Vec3ToJson(component.offset);
    e["collider"] = j;
  }

  if (entity.HasComponent<ColliderGroupComponent>()) {
    const auto &group = entity.GetComponent<ColliderGroupComponent>();
    json        arr   = json::array();
    for (const auto &s : group.shapes) {
      json j;
      switch (s.shape) {
        case ColliderShapeData::Shape::Sphere: j["shape"] = "sphere"; break;
        case ColliderShapeData::Shape::Capsule: j["shape"] = "capsule"; break;
        case ColliderShapeData::Shape::Cylinder: j["shape"] = "cylinder"; break;
        case ColliderShapeData::Shape::Box:
        default: j["shape"] = "box"; break;
      }
      j["half_extents"] = Vec3ToJson(s.box_half_extents);
      j["offset"]       = Vec3ToJson(s.offset);
      switch (s.shape) {
        case ColliderShapeData::Shape::Sphere: j["radius"] = s.sphere_radius; break;
        case ColliderShapeData::Shape::Capsule:
          j["radius"] = s.capsule_radius;
          j["half_height"] = s.capsule_half_height;
          break;
        case ColliderShapeData::Shape::Cylinder:
          j["radius"] = s.cylinder_radius;
          j["half_height"] = s.cylinder_half_height;
          break;
        case ColliderShapeData::Shape::Box:
        default: break;
      }
      arr.push_back(std::move(j));
    }
    e["collider_group"] = std::move(arr);
  }

  if (entity.HasComponent<CameraController>()) {
    const auto &component = entity.GetComponent<CameraController>();
    json        j;
    j["move_speed"]       = component.move_speed;
    j["look_sensitivity"] = component.look_sensitivity;
    e["camera_controller"] = j;
  }

  if (entity.HasComponent<LuaScriptComponent>()) {
    e["lua_script"] = entity.GetComponent<LuaScriptComponent>().path;
  }

  if (entity.HasComponent<AnimationComponent>()) {
    const auto &a = entity.GetComponent<AnimationComponent>();
    json        j;
    const auto write_channel = [](json &target, const char *name, const std::vector<Keyframe> &keys) {
      json arr = json::array();
      for (const auto &k : keys) {
        json kk;
        kk["time"]  = k.time;
        kk["value"] = Vec3ToJson(k.value);
        arr.push_back(std::move(kk));
      }
      target[name] = std::move(arr);
    };
    write_channel(j, "translation", a.translation_keys);
    write_channel(j, "rotation", a.rotation_keys);
    write_channel(j, "scale", a.scale_keys);
    e["animation"] = std::move(j);
  }

  return e;
}

/// @brief Creates an entity in `scene` from its serialized JSON form.
Entity LoadEntityFromJson(Scene &scene, const json &e) {
  Entity entity = scene.CreateEntity(e.value("tag", "Unnamed Entity"));

  if (e.contains("transform")) {
    const auto &j = e["transform"];
    auto       &t = entity.AddComponent<Transform>();
    t.translation = Vec3FromJson(j.value("translation", json()));
    t.rotation    = Vec3FromJson(j.value("rotation", json()));
    t.scale       = Vec3FromJson(j.value("scale", json()), glm::vec3(1.0f));
  }

  if (e.contains("mesh")) {
    const auto &j    = e["mesh"];
    Ref<Mesh>   mesh = MeshFromSource(j.value("source", ""));
    if (mesh) {
      entity.AddComponent<MeshComponent>(mesh, MaterialFromJson(j.value("material", json())));
    }
  }

  if (e.contains("camera")) {
    const auto &j         = e["camera"];
    CameraComponent component;
    CameraFromJson(j, component.camera);
    component.primary = j.value("primary", false);
    entity.AddComponent<CameraComponent>(component);
  }

  if (e.contains("rigid_body")) {
    const auto &j = e["rigid_body"];
    RigidBodyComponent component;
    component.type        = j.value("type", "dynamic") == "static" ? RigidBodyComponent::Type::Static
                                                                   : RigidBodyComponent::Type::Dynamic;
    component.friction    = j.value("friction", 0.5f);
    component.restitution = j.value("restitution", 0.0f);
    component.continuous_collision = j.value("continuous_collision", false);
    component.is_sensor             = j.value("sensor", false);
    entity.AddComponent<RigidBodyComponent>(component);
  }

  if (e.contains("collider")) {
    const auto &j = e["collider"];
    const std::string shape_name = j.value("shape", "box");
    ColliderComponent component;
    component.shape = shape_name == "sphere"   ? ColliderComponent::Shape::Sphere
                      : shape_name == "capsule" ? ColliderComponent::Shape::Capsule
                      : shape_name == "cylinder" ? ColliderComponent::Shape::Cylinder
                                                 : ColliderComponent::Shape::Box;
    component.box_half_extents  = Vec3FromJson(j.value("half_extents", json()), glm::vec3(0.5f));
    const float radius          = j.value("radius", 0.5f);
    component.sphere_radius     = radius;
    component.capsule_radius    = radius;
    component.cylinder_radius   = radius;
    component.capsule_half_height  = j.value("half_height", component.capsule_half_height);
    component.cylinder_half_height = j.value("half_height", component.cylinder_half_height);
    component.offset            = Vec3FromJson(j.value("offset", json()));
    entity.AddComponent<ColliderComponent>(component);
  }

  if (e.contains("collider_group") && e["collider_group"].is_array()) {
    ColliderGroupComponent group;
    for (const auto &js : e["collider_group"]) {
      const std::string shape_name = js.value("shape", "box");
      ColliderShapeData  s;
      s.shape = shape_name == "sphere"   ? ColliderShapeData::Shape::Sphere
                : shape_name == "capsule" ? ColliderShapeData::Shape::Capsule
                : shape_name == "cylinder" ? ColliderShapeData::Shape::Cylinder
                                          : ColliderShapeData::Shape::Box;
      s.box_half_extents = Vec3FromJson(js.value("half_extents", json()), glm::vec3(0.5f));
      s.offset           = Vec3FromJson(js.value("offset", json()));
      const float radius = js.value("radius", 0.5f);
      const float hh     = js.value("half_height", 0.5f);
      switch (s.shape) {
        case ColliderShapeData::Shape::Sphere: s.sphere_radius = radius; break;
        case ColliderShapeData::Shape::Capsule:
          s.capsule_radius = radius;
          s.capsule_half_height = hh;
          break;
        case ColliderShapeData::Shape::Cylinder:
          s.cylinder_radius = radius;
          s.cylinder_half_height = hh;
          break;
        case ColliderShapeData::Shape::Box:
        default: break;
      }
      group.shapes.push_back(s);
    }
    if (!group.Empty()) {
      entity.AddComponent<ColliderGroupComponent>(std::move(group));
    }
  }

  if (e.contains("camera_controller")) {
    const auto &j = e["camera_controller"];
    CameraController component;
    component.move_speed       = j.value("move_speed", 5.0f);
    component.look_sensitivity = j.value("look_sensitivity", 0.15f);
    entity.AddComponent<CameraController>(component);
  }

  if (e.contains("lua_script") && e["lua_script"].is_string()) {
    entity.AddComponent<LuaScriptComponent>(e["lua_script"].get<std::string>());
  }

  if (e.contains("animation") && e["animation"].is_object()) {
    const auto &j = e["animation"];
    const auto read_channel = [](const json &src, const char *name) {
      std::vector<Keyframe> out;
      if (src.contains(name) && src[name].is_array()) {
        for (const auto &kk : src[name]) {
          Keyframe key;
          key.time  = kk.value("time", 0.0f);
          key.value = Vec3FromJson(kk.value("value", json()));
          out.push_back(key);
        }
      }
      return out;
    };
    AnimationComponent animation;
    animation.translation_keys = read_channel(j, "translation");
    animation.rotation_keys    = read_channel(j, "rotation");
    animation.scale_keys       = read_channel(j, "scale");
    if (!animation.Empty()) {
      entity.AddComponent<AnimationComponent>(std::move(animation));
    }
  }

  return entity;
}

/// @brief Creates every entity from a serialized `entities` array. Parenting is
/// resolved in a second pass (by document index) so a child may appear before
/// its parent in the file. Scenes without a `parent` field stay root-level,
/// keeping old files fully backward compatible.
void LoadEntitiesFromJson(Scene &scene, const json &array) {
  if (!array.is_array()) return;

  std::vector<entt::entity> created;
  created.reserve(array.size());
  for (const auto &e : array) {
    if (!e.is_object()) {
      created.push_back(entt::null);
      continue;
    }
    created.push_back(LoadEntityFromJson(scene, e).GetHandle());
  }

  for (size_t i = 0; i < array.size() && i < created.size(); ++i) {
    const auto &e = array[i];
    if (!e.is_object() || created[i] == entt::null || !e.contains("parent") ||
        !e["parent"].is_number_integer()) {
      continue;
    }
    const int parent_index = e["parent"].get<int>();
    if (parent_index >= 0 && static_cast<size_t>(parent_index) < created.size()) {
      scene.SetParent(created[i], created[parent_index]);
    }
  }
}

}  // namespace

void Scene::SaveScene(const std::string &path) {
  json root;
  root["version"] = 1;

  // Directional light.
  {
    const auto &light = renderer_->GetLight();
    json        j;
    j["direction"] = Vec3ToJson(light.direction);
    j["color"]     = Vec3ToJson(light.color);
    root["directional_light"] = j;
  }

  // Point lights.
  {
    json lights = json::array();
    for (const auto &light : renderer_->GetPointLights()) {
      json j;
      j["position"]     = Vec3ToJson(light.position);
      j["color"]        = Vec3ToJson(light.color);
      j["intensity"]    = light.intensity;
      j["radius"]       = light.radius;
      j["casts_shadow"] = light.casts_shadow;
      lights.push_back(j);
    }
    root["point_lights"] = lights;
  }

  // Spot lights.
  {
    json lights = json::array();
    for (const auto &light : renderer_->GetSpotLights()) {
      json j;
      j["position"]     = Vec3ToJson(light.position);
      j["direction"]    = Vec3ToJson(light.direction);
      j["color"]        = Vec3ToJson(light.color);
      j["intensity"]    = light.intensity;
      j["range"]        = light.range;
      j["cutoff"]       = light.cutoff;
      j["outer_cutoff"] = light.outer_cutoff;
      lights.push_back(j);
    }
    root["spot_lights"] = lights;
  }

  // Render settings.
  {
    json j;
    j["render_mode"]     = RenderModeToString(GetRenderMode());
    j["exposure"]        = GetExposure();
    j["bloom_enabled"]   = IsBloomEnabled();
    j["bloom_strength"]  = GetBloomStrength();
    j["bloom_threshold"] = GetBloomThreshold();
    j["god_rays"]        = GetGodRaysStrength();
    j["ssao"]            = IsSSAOEnabled();
    j["taa"]             = IsTAAEnabled();
    j["ibl_intensity"]   = GetIblIntensity();
    j["pcf_radius"]      = GetShadowPcfRadius();
    root["render"]       = j;
  }

  // Scene-level animation timeline settings (persisted so standalone playback
  // matches the editor's Loop toggle).
  if (HasAnyAnimation()) {
    json j;
    j["loop"] = anim_loop_;
    root["animation"] = std::move(j);
  }

  if (!main_script_.empty()) {
    root["main_script"] = main_script_;
  }

  // Entities. The array is written in creation order; each entity may carry an
  // optional "parent" index into this same array (see EntityToJson).
  {
    std::vector<Entity>        content  = ContentEntities(*this);
    std::unordered_map<entt::entity, int> index_of;
    index_of.reserve(content.size());
    for (size_t i = 0; i < content.size(); ++i) {
      index_of[content[i].GetHandle()] = static_cast<int>(i);
    }

    json entity_array = json::array();
    for (auto &entity : content) {
      int parent_index = -1;
      const entt::entity parent = GetParent(entity.GetHandle());
      if (parent != entt::null) {
        const auto it = index_of.find(parent);
        if (it != index_of.end()) {
          parent_index = it->second;
        }
      }
      entity_array.push_back(EntityToJson(entity, parent_index));
    }
    root["entities"] = entity_array;
  }

  std::ofstream out(path);
  if (!out) {
    LOG_ERROR("Scene") << "Failed to open scene file for writing: " << path;
    return;
  }
  out << root.dump(2);
  LOG_INFO("Scene") << "Saved scene to " << path;
}

void Scene::LoadScene(const std::string &path) {
  std::ifstream in(path);
  if (!in) {
    LOG_ERROR("Scene") << "Failed to open scene file: " << path;
    return;
  }

  json root;
  in >> root;

  // Clear the current scene state before rebuilding it from the file. The
  // script engine must be reset too: its instances reference the old entities
  // and main script (their OnDestroy hooks fire here, before the registry is
  // wiped). Per-entity instances are re-synced lazily from the new components
  // on the next StartAll/Update.
  script_engine_->Clear();
  main_script_.clear();
  registry_.clear();
  entities_.clear();
  renderer_->ClearPointLights();
  renderer_->ClearSpotLights();
  anim_time_    = 0.0f;
  anim_playing_ = false;

  // Directional light.
  if (root.contains("directional_light")) {
    const auto &j = root["directional_light"];
    renderer_->GetLight().direction = Vec3FromJson(j.value("direction", json()), glm::vec3(-0.3f, -1.0f, -0.4f));
    renderer_->GetLight().color     = Vec3FromJson(j.value("color", json()), glm::vec3(2.5f));
  }

  // Point lights.
  if (root.contains("point_lights") && root["point_lights"].is_array()) {
    for (const auto &j : root["point_lights"]) {
      PointLight light;
      light.position     = Vec3FromJson(j.value("position", json()));
      light.color        = Vec3FromJson(j.value("color", json()), glm::vec3(1.0f));
      light.intensity    = j.value("intensity", 1.0f);
      light.radius       = j.value("radius", 4.0f);
      light.casts_shadow = j.value("casts_shadow", false);
      renderer_->AddPointLight(light);
    }
  }

  // Spot lights.
  if (root.contains("spot_lights") && root["spot_lights"].is_array()) {
    for (const auto &j : root["spot_lights"]) {
      SpotLight light;
      light.position     = Vec3FromJson(j.value("position", json()));
      light.direction    = Vec3FromJson(j.value("direction", json()), glm::vec3(0.0f, -1.0f, 0.0f));
      light.color        = Vec3FromJson(j.value("color", json()), glm::vec3(1.0f));
      light.intensity    = j.value("intensity", 1.0f);
      light.range        = j.value("range", 8.0f);
      light.cutoff       = j.value("cutoff", light.cutoff);
      light.outer_cutoff = j.value("outer_cutoff", light.outer_cutoff);
      renderer_->AddSpotLight(light);
    }
  }

  // Render settings.
  if (root.contains("render")) {
    const auto &j = root["render"];
    SetRenderMode(RenderModeFromString(j.value("render_mode", "lit")));
    SetExposure(j.value("exposure", 1.0f));
    SetBloomEnabled(j.value("bloom_enabled", false));
    SetBloomStrength(j.value("bloom_strength", 0.015f));
    SetBloomThreshold(j.value("bloom_threshold", 1.0f));
    SetGodRaysStrength(j.value("god_rays", 0.06f));
    SetSSAOEnabled(j.value("ssao", true));
    SetTAAEnabled(j.value("taa", true));
    SetIblIntensity(j.value("ibl_intensity", 0.8f));
    SetShadowPcfRadius(j.value("pcf_radius", 4.0f));
  }

  if (root.contains("animation") && root["animation"].is_object()) {
    anim_loop_ = root["animation"].value("loop", true);
  }

  // Entities.
  LoadEntitiesFromJson(*this, root.value("entities", json()));

  // Scene main script (loaded after the entities it may reference).
  main_script_ = root.value("main_script", "");
  if (!main_script_.empty()) {
    script_engine_->LoadMainScript(main_script_);
  }

  LOG_INFO("Scene") << "Loaded scene from " << path << " (" << entities_.size() << " entities)";
}

void Scene::CapturePlaySnapshot() {
  std::vector<Entity>        content  = ContentEntities(*this);
  std::unordered_map<entt::entity, int> index_of;
  index_of.reserve(content.size());
  for (size_t i = 0; i < content.size(); ++i) {
    index_of[content[i].GetHandle()] = static_cast<int>(i);
  }

  json entity_array = json::array();
  for (auto &entity : content) {
    int parent_index = -1;
    const entt::entity parent = GetParent(entity.GetHandle());
    if (parent != entt::null) {
      const auto it = index_of.find(parent);
      if (it != index_of.end()) {
        parent_index = it->second;
      }
    }
    entity_array.push_back(EntityToJson(entity, parent_index));
  }
  play_snapshot_ = entity_array.dump();
  LOG_DEBUG("Scene") << "Captured play snapshot (" << entity_array.size() << " entities)";
}

void Scene::RestorePlaySnapshot() {
  if (play_snapshot_.empty()) return;

  json entity_array;
  try {
    entity_array = json::parse(play_snapshot_);
  } catch (...) {
    LOG_ERROR("Scene") << "Failed to parse play snapshot; scene left as-is";
    play_snapshot_.clear();
    return;
  }

  // Remove every content entity currently present (editor-only helpers such as
  // the grid are kept). This also drops anything scripts spawned during Play
  // and re-creates anything they destroyed.
  RemoveContentEntities();

  const size_t restored = entity_array.is_array() ? entity_array.size() : 0;
  LoadEntitiesFromJson(*this, entity_array);

  LOG_INFO("Scene") << "Restored play snapshot (" << restored << " content entities)";
  play_snapshot_.clear();
}

void Scene::RemoveContentEntities() {
  std::vector<entt::entity> to_destroy;
  for (auto &entity : entities_) {
    const auto *tag = entity.HasComponent<Tag>() ? &entity.GetComponent<Tag>() : nullptr;
    if (tag && tag->editor_only) continue;
    to_destroy.push_back(entity.GetHandle());
  }
  for (const entt::entity handle : to_destroy) {
    DestroyEntity(Entity(handle, &registry_));
  }
}

void Scene::ClearContent() {
  StopSimulationIfRunning();
  script_engine_->Clear();
  main_script_.clear();
  physics_world_->ResetContacts();
  renderer_->ClearPointLights();
  renderer_->ClearSpotLights();
  anim_time_    = 0.0f;
  anim_playing_ = false;
  RemoveContentEntities();
  LOG_INFO("Scene") << "New (empty) scene created — content cleared, editor helpers kept";
}

bool Scene::OpenSceneFile(const std::string &path) {
  std::ifstream in(path);
  if (!in) {
    LOG_ERROR("Scene") << "Failed to open scene file: " << path;
    return false;
  }

  json root;
  try {
    in >> root;
  } catch (...) {
    LOG_ERROR("Scene") << "Failed to parse scene file: " << path;
    return false;
  }

  StopSimulationIfRunning();
  script_engine_->Clear();
  main_script_.clear();
  physics_world_->ResetContacts();
  renderer_->ClearPointLights();
  renderer_->ClearSpotLights();
  anim_time_    = 0.0f;
  anim_playing_ = false;
  RemoveContentEntities();

  // Directional light.
  if (root.contains("directional_light")) {
    const auto &j = root["directional_light"];
    renderer_->GetLight().direction = Vec3FromJson(j.value("direction", json()), glm::vec3(-0.3f, -1.0f, -0.4f));
    renderer_->GetLight().color     = Vec3FromJson(j.value("color", json()), glm::vec3(2.5f));
  }

  // Point lights.
  if (root.contains("point_lights") && root["point_lights"].is_array()) {
    for (const auto &j : root["point_lights"]) {
      PointLight light;
      light.position     = Vec3FromJson(j.value("position", json()));
      light.color        = Vec3FromJson(j.value("color", json()), glm::vec3(1.0f));
      light.intensity    = j.value("intensity", 1.0f);
      light.radius       = j.value("radius", 4.0f);
      light.casts_shadow = j.value("casts_shadow", false);
      renderer_->AddPointLight(light);
    }
  }

  // Spot lights.
  if (root.contains("spot_lights") && root["spot_lights"].is_array()) {
    for (const auto &j : root["spot_lights"]) {
      SpotLight light;
      light.position     = Vec3FromJson(j.value("position", json()));
      light.direction    = Vec3FromJson(j.value("direction", json()), glm::vec3(0.0f, -1.0f, 0.0f));
      light.color        = Vec3FromJson(j.value("color", json()), glm::vec3(1.0f));
      light.intensity    = j.value("intensity", 1.0f);
      light.range        = j.value("range", 8.0f);
      light.cutoff       = j.value("cutoff", light.cutoff);
      light.outer_cutoff = j.value("outer_cutoff", light.outer_cutoff);
      renderer_->AddSpotLight(light);
    }
  }

  // Render settings.
  if (root.contains("render")) {
    const auto &j = root["render"];
    SetRenderMode(RenderModeFromString(j.value("render_mode", "lit")));
    SetExposure(j.value("exposure", 1.0f));
    SetBloomEnabled(j.value("bloom_enabled", false));
    SetBloomStrength(j.value("bloom_strength", 0.015f));
    SetBloomThreshold(j.value("bloom_threshold", 1.0f));
    SetGodRaysStrength(j.value("god_rays", 0.06f));
    SetSSAOEnabled(j.value("ssao", true));
    SetTAAEnabled(j.value("taa", true));
    SetIblIntensity(j.value("ibl_intensity", 0.8f));
    SetShadowPcfRadius(j.value("pcf_radius", 4.0f));
  }

  if (root.contains("animation") && root["animation"].is_object()) {
    anim_loop_ = root["animation"].value("loop", true);
  }

  // Entities.
  LoadEntitiesFromJson(*this, root.value("entities", json()));

  main_script_ = root.value("main_script", "");
  LOG_INFO("Scene") << "Opened scene file " << path << " (" << entities_.size() << " entities)";
  return true;
}

}  // namespace MEngine
