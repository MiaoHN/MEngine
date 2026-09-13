#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "core/common.hpp"

namespace MEngine {

class Shader;
class Texture;
class Mesh;
class ShaderLibrary;
class TextureLibrary;

/**
 * @brief Central resource manager (single source of truth for assets).
 *
 * Loads and caches shaders / textures by logical name or path, resolving them
 * against a configurable asset root directory. A `manifest.json` at the asset
 * root maps logical names to files (Unity `.meta` / UE AssetRegistry style),
 * with a naming-convention fallback. Loading failures fall back to default
 * resources so rendering never breaks.
 */
class AssetManager {
 public:
  static AssetManager &Instance();

  ~AssetManager();

  AssetManager(const AssetManager &)            = delete;
  AssetManager &operator=(const AssetManager &) = delete;

  /// @brief Sets the asset root and (re)loads its manifest.json.
  void SetAssetRoot(const std::string &root);

  [[nodiscard]] const std::string &GetAssetRoot() const { return asset_root_; }

  /// @brief Resolves a path relative to the asset root. A path that is already
  /// absolute is returned unchanged (resolving twice must never happen: the
  /// asset root would be prefixed again and the file would not be found).
  [[nodiscard]] std::string Resolve(const std::string &relative) const;

  /// @brief Loads (and caches) a shader by logical name ("pbr", "taa", ...).
  Ref<Shader> GetShader(const std::string &name);

  /// @brief Loads (and caches) a texture. `name_or_path` is a manifest name or
  /// a path that is understood both ways: relative to the asset root
  /// ("textures/checker.png") or already resolved/absolute. The latter keeps
  /// callers that pass a resolved path (or a path read back from a scene file)
  /// from prefixing the root twice - the usual cause of sprites suddenly showing
  /// the magenta "missing texture" fallback after a play / stop round-trip.
  /// @brief Loads (and caches) a texture. When `srgb` is true the bytes are
  /// uploaded as an sRGB texture so the GPU decodes sRGB->linear on sample
  /// (LearnOpenGL loads its albedo maps this way for gamma-correct pipelines).
  /// The cache key includes the srgb flag, so the same file can be used raw in
  /// one scene and sRGB in another.
  Ref<Texture> GetTexture(const std::string &name_or_path, bool srgb = false);

  /// @brief Returns a shared mesh for a serialized source string ("cube",
  /// "plane", "sphere" or a model file path relative to the asset root).
  /// Same-source meshes share one GPU object, which lets the renderer batch
  /// them into single instanced draws.
  Ref<Mesh> GetMesh(const std::string &source);

  /// @brief The fallback shader / texture used when a load fails.
  Ref<Shader> GetDefaultShader();
  Ref<Texture> GetDefaultTexture();

 private:
  AssetManager() = default;
  void LoadManifest();

  std::string asset_root_;

  // name -> {vert_path, frag_path} (relative to the asset root)
  std::unordered_map<std::string, std::pair<std::string, std::string>> shader_manifest_;
  // name -> relative path
  std::unordered_map<std::string, std::string> texture_manifest_;

  std::unique_ptr<ShaderLibrary>  shader_library_;
  std::unique_ptr<TextureLibrary> texture_library_;
  // source string -> shared mesh (primitives + loaded model files)
  std::unordered_map<std::string, Ref<Mesh>> mesh_cache_;

  Ref<Shader>  default_shader_;
  Ref<Texture> default_texture_;
};

}  // namespace MEngine
