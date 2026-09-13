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
#include <stb_image.h>

#include "core/logger.hpp"
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

/// @brief LearnOpenGL-exact Blinn material for an untextured object: albedo =
/// `color`, optional explicit specular color (LO's material.specular vec3).
/// Uses the dedicated "blinn_lo" shader (LO's exact per-light math).
inline Ref<Material> BlinnLo(const glm::vec3 &color, const glm::vec3 &specular_color = glm::vec3(1.0f),
                             float shininess = 32.0f) {
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(AssetManager::Instance().GetShader("blinn_lo"));
  m->SetBaseColorFactor(glm::vec4(color, 1.0f));
  m->SetSpecularColor(specular_color);
  m->SetShininess(shininess);
  return m;
}

/// @brief LearnOpenGL-exact textured Blinn material: container2-style diffuse
/// map + a per-pixel specular map (LO's material.specular sampler).
inline Ref<Material> BlinnLoTextured(const std::string &albedo_path, const std::string &specular_path,
                                     float shininess = 32.0f) {
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(AssetManager::Instance().GetShader("blinn_lo"));
  m->SetAlbedoMap(AssetManager::Instance().GetTexture(albedo_path));
  m->SetSpecularMap(AssetManager::Instance().GetTexture(specular_path));
  m->SetShininess(shininess);
  return m;
}

/// @brief LearnOpenGL-exact textured Blinn material with a normal map (LO
/// 4.normal_mapping): albedo + brickwall-style normal map. `spec_intensity`
/// is the scalar specular strength (LO uses a grey vec3(0.2) specular -> 0.2).
/// Renders double-sided (the LO wall is a two-triangle quad with no culling).
inline Ref<Material> BlinnLoNormalMapped(const std::string &albedo_path, const std::string &normal_path,
                                         float shininess = 32.0f, float spec_intensity = 0.2f) {
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(AssetManager::Instance().GetShader("blinn_lo"));
  m->SetAlbedoMap(AssetManager::Instance().GetTexture(albedo_path));
  m->SetNormalMap(AssetManager::Instance().GetTexture(normal_path));
  m->SetSpecularFactor(spec_intensity);
  m->SetShininess(shininess);
  m->SetCullMode(CullMode::None);
  return m;
}

/// @brief LearnOpenGL-exact textured Blinn material with just a diffuse map
/// (wood floor etc.). LO 7.bloom's shader has no specular term, so no spec map.
/// `srgb` mirrors LO's loadTexture(..., true): the map is decoded sRGB->linear
/// on the GPU (LO 7.bloom loads its wood/container albedo maps this way).
inline Ref<Material> BlinnLoDiffuse(const std::string &albedo_path, float shininess = 32.0f,
                                    bool srgb = false) {
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(AssetManager::Instance().GetShader("blinn_lo"));
  m->SetAlbedoMap(AssetManager::Instance().GetTexture(albedo_path, srgb));
  if (srgb) {
    m->SetAlbedoSRGB(true);  // decode the map sRGB->linear in the shader
  }
  m->SetShininess(shininess);
  return m;
}

/// @brief Switches a scene to LearnOpenGL-exact parity: per-light
/// ambient/diffuse/specular lighting, raw linear composite output, no
/// skybox/IBL/tone/gamma, plain dark background (LO clears to 0.1), no TAA /
/// bloom / SSAO. `background` mirrors LO's glClearColor.
inline void LoScene(Scene &scene, const glm::vec3 &background = glm::vec3(0.1f, 0.1f, 0.1f)) {
  scene.SetLoLighting(true);
  scene.SetLinearOutput(true);
  scene.SetSkyboxEnabled(false);
  scene.SetBackgroundColor(background);
  scene.SetIblIntensity(0.0f);
  scene.SetExposure(1.0f);
  scene.SetTAAEnabled(false);
  scene.SetBloomEnabled(false);
  scene.SetSSAOEnabled(false);
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

/// @brief A small emissive "lamp" cube at `pos` (LO's light_cube) - pure
/// `color`, unaffected by lighting.
inline void Lamp(Scene &scene, const glm::vec3 &pos, const glm::vec3 &color = glm::vec3(1.0f),
                 float scale = 0.2f) {
  Entity e = scene.CreateEntity("lamp");
  auto &t  = e.AddComponent<Transform>();
  t.translation = pos;
  t.scale       = glm::vec3(scale);
  e.AddComponent<MeshComponent>(Mesh::CreateCube(), Unlit(color));
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

/// @brief Turns the scene's default directional light OFF (LO-exact scenes
/// that have no sun - 2.2/3.1/4.2/5.3 - must call this, otherwise the engine's
/// default directional ambient/diffuse/specular would add a spurious sun).
inline void NoSun(Scene &scene) {
  auto &l = scene.GetLight();
  l.ambient  = glm::vec3(0.0f);
  l.diffuse  = glm::vec3(0.0f);
  l.specular = glm::vec3(0.0f);
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

namespace detail {

/// @brief Loads an image (TGA/PNG/...) as an RGBA8 GPU texture WITHOUT a
/// vertical flip. glTF meshes use a top-left UV origin, so raw file rows
/// (row 0 = top) must be uploaded as-is to match. stb's implementation lives
/// in the engine's texture.cpp, which every example links against.
inline Ref<Texture> LoadRawTexture(const std::string &path) {
  stbi_set_flip_vertically_on_load(false);  // glTF UV convention
  int      w = 0, h = 0, c = 0;
  stbi_uc *pixels = stbi_load(path.c_str(), &w, &h, &c, 4);
  if (!pixels || w <= 0 || h <= 0) {
    LOG_ERROR("examples") << "PbrSidecarTextured: failed to load '" << path << "'";
    if (pixels) {
      stbi_image_free(pixels);
    }
    return nullptr;
  }
  Ref<Texture> tex = CreateRef<Texture>();
  tex->SetData(pixels, w, h);
  stbi_image_free(pixels);
  return tex;
}

/// @brief Packs separate single-channel roughness + metallic maps into the
/// engine's combined MR texture: R unused(=1), G=roughness, B=metallic
/// (pbr_frag samples roughness from G and metallic from B).
inline Ref<Texture> PackMetallicRoughness(const std::string &roughness_path,
                                          const std::string &metallic_path) {
  stbi_set_flip_vertically_on_load(false);
  int rw = 0, rh = 0, rc = 0, mw = 0, mh = 0, mc = 0;
  stbi_uc *r = stbi_load(roughness_path.c_str(), &rw, &rh, &rc, 0);
  stbi_uc *m = stbi_load(metallic_path.c_str(), &mw, &mh, &mc, 0);
  if (!r || !m || rw != mw || rh != mh) {
    LOG_ERROR("examples") << "PackMetallicRoughness: size mismatch / load failure "
                           << "(rough " << (r ? rw : -1) << "x" << (r ? rh : -1) << ", metal "
                           << (m ? mw : -1) << "x" << (m ? mh : -1) << ")";
    if (r) {
      stbi_image_free(r);
    }
    if (m) {
      stbi_image_free(m);
    }
    return nullptr;
  }
  std::vector<unsigned char> out(static_cast<size_t>(rw) * static_cast<size_t>(rh) * 4);
  for (int i = 0; i < rw * rh; ++i) {
    out[i * 4 + 0] = 255;        // R unused
    out[i * 4 + 1] = r[i * rc];  // G = roughness
    out[i * 4 + 2] = m[i * mc];  // B = metallic
    out[i * 4 + 3] = 255;
  }
  Ref<Texture> tex = CreateRef<Texture>();
  tex->SetData(out.data(), rw, rh);
  stbi_image_free(r);
  stbi_image_free(m);
  return tex;
}

}  // namespace detail

/// @brief A full PBR material assembled from separate sidecar image maps - the
/// extra texture files that FBX2glTF-style converters leave next to a GLB when
/// the glTF only embeds the albedo (e.g. Cerberus keeps its metallic /
/// roughness / normal / AO in sibling files). The albedo map is treated as
/// sRGB and decoded in the shader; metallic+roughness are packed into the
/// engine MR texture; every map is read without a vertical flip to match the
/// glTF mesh UVs. Returns a material with null maps (engine defaults) if a
/// file is missing, so callers should check their textures loaded.
inline Ref<Material> PbrSidecarTextured(const std::string &albedo_path, const std::string &normal_path,
                                        const std::string &roughness_path,
                                        const std::string &metallic_path,
                                        const std::string &ao_path = "") {
  Ref<Material> m = CreateRef<Material>();
  m->SetShader(PbrShader());
  m->SetAlbedoMap(detail::LoadRawTexture(albedo_path));
  m->SetAlbedoSRGB(true);
  m->SetNormalMap(detail::LoadRawTexture(normal_path));
  m->SetMetallicRoughnessMap(detail::PackMetallicRoughness(roughness_path, metallic_path));
  if (!ao_path.empty()) {
    m->SetAOMap(detail::LoadRawTexture(ao_path));
  }
  m->SetMetallicFactor(1.0f);  // maps drive metallic/roughness per pixel
  m->SetRoughnessFactor(1.0f);
  return m;
}

}  // namespace examples
}  // namespace MEngine
