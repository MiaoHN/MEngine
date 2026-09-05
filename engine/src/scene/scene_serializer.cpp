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
Ref<Mesh> MeshFromSource(const std::string &source) {
  if (source == "cube") return Mesh::CreateCube();
  if (source == "plane") return Mesh::CreatePlane();
  if (source == "sphere") return Mesh::CreateSphere();
  if (source.empty()) return nullptr;

  const std::string resolved = ResolveAsset(source);
  const std::string ext      = std::filesystem::path(resolved).extension().string();

  Ref<Mesh> mesh;
  if (ext == ".obj") {
    mesh = ModelLoader::LoadObj(resolved);
  } else if (ext == ".gltf" || ext == ".glb") {
    mesh = ModelLoader::LoadGltf(resolved);
  }
  if (mesh) {
    mesh->SetSource(source);
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

  // Entities.
  {
    json entity_array = json::array();
    for (auto &entity : entities_) {
      const auto &tag = entity.GetComponent<Tag>();
      if (tag.editor_only) continue;

      json e;
      e["tag"] = tag.tag;

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
        e["rigid_body"]  = j;
      }

      if (entity.HasComponent<ColliderComponent>()) {
        const auto &component = entity.GetComponent<ColliderComponent>();
        json        j;
        j["shape"]         = component.shape == ColliderComponent::Shape::Sphere ? "sphere" : "box";
        j["half_extents"]  = Vec3ToJson(component.box_half_extents);
        j["radius"]        = component.sphere_radius;
        j["offset"]        = Vec3ToJson(component.offset);
        e["collider"]      = j;
      }

      entity_array.push_back(e);
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

  // Clear the current scene state before rebuilding it from the file.
  registry_.clear();
  entities_.clear();
  renderer_->ClearPointLights();
  renderer_->ClearSpotLights();

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
    SetIblIntensity(j.value("ibl_intensity", 0.4f));
    SetShadowPcfRadius(j.value("pcf_radius", 4.0f));
  }

  // Entities.
  if (root.contains("entities") && root["entities"].is_array()) {
    for (const auto &e : root["entities"]) {
      Entity entity = CreateEntity(e.value("tag", "Unnamed Entity"));

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
        entity.AddComponent<RigidBodyComponent>(component);
      }

      if (e.contains("collider")) {
        const auto &j = e["collider"];
        ColliderComponent component;
        component.shape             = j.value("shape", "box") == "sphere" ? ColliderComponent::Shape::Sphere
                                                                          : ColliderComponent::Shape::Box;
        component.box_half_extents  = Vec3FromJson(j.value("half_extents", json()), glm::vec3(0.5f));
        component.sphere_radius     = j.value("radius", 0.5f);
        component.offset            = Vec3FromJson(j.value("offset", json()));
        entity.AddComponent<ColliderComponent>(component);
      }
    }
  }

  LOG_INFO("Scene") << "Loaded scene from " << path << " (" << entities_.size() << " entities)";
}

}  // namespace MEngine
