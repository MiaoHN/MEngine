/**
 * @file example_helpers.hpp
 * @brief Small shared helpers for building example scenes with the MEngine
 * public API (PBR materials + primitives). Header-only so every example
 * executable can include exactly what it needs.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/asset_manager.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/texture.hpp"
#include "scene/entity.hpp"
#include "scene/scene.hpp"

namespace MEngine {
namespace examples {

/// @brief Shared PBR shader (from the asset manifest).
inline Ref<Shader> PbrShader() { return AssetManager::Instance().GetShader("pbr"); }

/// @brief A plain PBR material with the given base color + metallic/roughness.
inline Ref<Material> Pbr(const glm::vec3 &color, float metallic, float roughness) {
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(PbrShader());
  m->SetBaseColorFactor(glm::vec4(color, 1.0f));
  m->SetMetallicFactor(metallic);
  m->SetRoughnessFactor(roughness);
  return m;
}

/// @brief A classic Blinn-Phong material (the second lighting pipeline). It
/// uses the "blinn" shader; `shininess` is the specular exponent and
/// `specular` its strength. Ambient is driven by the scene's ibl_intensity.
inline Ref<Material> Blinn(const glm::vec3 &color, float shininess = 32.0f, float specular = 0.5f) {
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(AssetManager::Instance().GetShader("blinn"));
  m->SetBaseColorFactor(glm::vec4(color, 1.0f));
  m->SetSpecularFactor(specular);
  m->SetShininess(shininess);
  return m;
}

/// @brief Blinn-Phong material with an albedo texture (diffuse map), used for
/// the LO lighting_maps / multiple_lights look.
inline Ref<Material> BlinnTextured(const std::string &albedo_path, float shininess = 32.0f,
                                   float specular = 0.5f) {
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(AssetManager::Instance().GetShader("blinn"));
  m->SetAlbedoMap(AssetManager::Instance().GetTexture(albedo_path));
  m->SetSpecularFactor(specular);
  m->SetShininess(shininess);
  return m;
}

/// @brief Unlit / emissive material (outputs `color` directly, no lighting) -
/// used for LearnOpenGL's small light-source cubes.
inline Ref<Material> Unlit(const glm::vec3 &color) {
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(PbrShader());
  m->SetBaseColorFactor(glm::vec4(color, 1.0f));
  m->SetUnlit(true);
  return m;
}

/// @brief A PBR material with an albedo texture (asset-relative path) and
/// optional normal map + roughness.
inline Ref<Material> PbrTextured(const std::string &albedo_path, const std::string &normal_path = "",
                                 float roughness = 0.7f, float metallic = 0.0f) {
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(PbrShader());
  m->SetAlbedoMap(AssetManager::Instance().GetTexture(albedo_path));
  if (!normal_path.empty()) {
    m->SetNormalMap(AssetManager::Instance().GetTexture(normal_path));
  }
  m->SetMetallicFactor(metallic);
  m->SetRoughnessFactor(roughness);
  return m;
}

/// @brief A +Y floor quad whose UVs repeat `tiles` times (for tiling textures).
inline Ref<Mesh> TiledPlane(float size, float tiles) {
  const float h = size * 0.5f;
  const std::vector<Vertex> verts = {
      {{-h, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
      {{h, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {tiles, 0.0f}},
      {{h, 0.0f, h}, {0.0f, 1.0f, 0.0f}, {tiles, tiles}},
      {{-h, 0.0f, h}, {0.0f, 1.0f, 0.0f}, {0.0f, tiles}},
  };
  const std::vector<unsigned int> idx = {0, 3, 2, 2, 1, 0};
  return Mesh::Create(verts, idx);
}

/// @brief A vertical +Z wall quad (for normal-mapping / texture demos).
inline Ref<Mesh> TiledWall(float width, float height, float tiles) {
  const float hw = width * 0.5f;
  const float hh = height * 0.5f;
  const std::vector<Vertex> verts = {
      {{-hw, -hh, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
      {{hw, -hh, 0.0f}, {0.0f, 0.0f, 1.0f}, {tiles, 0.0f}},
      {{hw, hh, 0.0f}, {0.0f, 0.0f, 1.0f}, {tiles, tiles}},
      {{-hw, hh, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, tiles}},
  };
  const std::vector<unsigned int> idx = {0, 1, 2, 0, 2, 3};
  return Mesh::Create(verts, idx);
}

/// @brief Adds a primitive mesh (cubes are unit-sized; pass `scale` for size).
inline void Put(Scene &scene, Ref<Mesh> mesh, Ref<Material> material, const glm::vec3 &pos, float scale = 1.0f) {
  Entity e = scene.CreateEntity("obj");
  auto &t  = e.AddComponent<Transform>();
  t.translation = pos;
  t.scale       = glm::vec3(scale);
  e.AddComponent<MeshComponent>(mesh, std::move(material));
}

/// @brief Like Put but applies an arbitrary axis-angle rotation (degrees) -
/// the exact form LearnOpenGL uses (`rotate(model, radians(angle), axis)`).
inline void PutAxis(Scene &scene, Ref<Mesh> mesh, Ref<Material> material, const glm::vec3 &pos,
                    const glm::vec3 &axis, float degrees, float scale = 1.0f) {
  Entity e = scene.CreateEntity("obj");
  auto &t  = e.AddComponent<Transform>();
  t.translation = pos;
  t.scale       = glm::vec3(scale);
  t.SetRotationAxisAngle(axis, degrees);
  e.AddComponent<MeshComponent>(mesh, std::move(material));
}

/// @brief Directional "sun" helper: direction travels away from the sun.
inline void Sun(Scene &scene, const glm::vec3 &travel_dir, const glm::vec3 &color) {
  scene.GetLight().direction = glm::normalize(travel_dir);
  scene.GetLight().color     = color;
}

/// @brief Matches the LearnOpenGL look: no skybox, a plain (usually dark)
/// solid background. The IBL environment still lights geometry - lower
/// `ibl` to keep the ambient small like the original tutorials.
inline void SolidBackground(Scene &scene, const glm::vec3 &background = glm::vec3(0.08f, 0.08f, 0.10f),
                            float ibl = 0.0f) {
  scene.SetSkyboxEnabled(false);
  scene.SetBackgroundColor(background);
  if (ibl >= 0.0f) {
    scene.SetIblIntensity(ibl);
  }
}

}  // namespace examples
}  // namespace MEngine
