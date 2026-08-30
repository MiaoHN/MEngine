#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "core/common.hpp"

namespace MEngine {

class Shader;
class Texture;
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

  /// @brief Resolves a path relative to the asset root.
  [[nodiscard]] std::string Resolve(const std::string &relative) const;

  /// @brief Loads (and caches) a shader by logical name ("pbr", "taa", ...).
  Ref<Shader> GetShader(const std::string &name);

  /// @brief Loads (and caches) a texture by name or path relative to the root.
  Ref<Texture> GetTexture(const std::string &name_or_path);

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

  Ref<Shader>  default_shader_;
  Ref<Texture> default_texture_;
};

}  // namespace MEngine
