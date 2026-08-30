#include "render/asset_manager.hpp"

#include <filesystem>
#include <fstream>

#include <json.hpp>

#include "core/logger.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"

namespace MEngine {

AssetManager &AssetManager::Instance() {
  static AssetManager instance;
  return instance;
}

AssetManager::~AssetManager() = default;

void AssetManager::SetAssetRoot(const std::string &root) {
  asset_root_ = root;
  if (!asset_root_.empty() && asset_root_.back() == '/') {
    asset_root_.pop_back();
  }
  if (!shader_library_) {
    shader_library_ = std::make_unique<ShaderLibrary>();
  }
  if (!texture_library_) {
    texture_library_ = std::make_unique<TextureLibrary>();
  }
  LoadManifest();
  LOG_INFO("AssetManager") << "Asset root set to: " << asset_root_;
}

std::string AssetManager::Resolve(const std::string &relative) const {
  if (relative.empty()) {
    return asset_root_;
  }
  if (asset_root_.empty()) {
    return relative;
  }
  return (std::filesystem::path(asset_root_) / relative).string();
}

void AssetManager::LoadManifest() {
  shader_manifest_.clear();
  texture_manifest_.clear();

  const std::string manifest_path = Resolve("manifest.json");
  std::ifstream     file(manifest_path);
  if (!file.is_open()) {
    LOG_WARN("AssetManager") << "No manifest at " << manifest_path << "; falling back to naming conventions.";
    return;
  }

  nlohmann::json manifest;
  file >> manifest;

  if (manifest.contains("shaders")) {
    for (const auto &[name, entry] : manifest["shaders"].items()) {
      shader_manifest_[name] = {entry[0].get<std::string>(), entry[1].get<std::string>()};
    }
  }
  if (manifest.contains("textures")) {
    for (const auto &[name, entry] : manifest["textures"].items()) {
      texture_manifest_[name] = entry.get<std::string>();
    }
  }
}

Ref<Shader> AssetManager::GetShader(const std::string &name) {
  if (shader_library_->Exists(name)) {
    return shader_library_->Get(name);
  }

  std::string vert_rel;
  std::string frag_rel;

  const auto it = shader_manifest_.find(name);
  if (it != shader_manifest_.end()) {
    vert_rel = it->second.first;
    frag_rel = it->second.second;
  } else {
    // Naming convention fallback: shaders/{name}_vert.glsl + {name}_frag.glsl.
    vert_rel = "shaders/" + name + "_vert.glsl";
    frag_rel = "shaders/" + name + "_frag.glsl";
  }

  return shader_library_->Load(name, Resolve(vert_rel), Resolve(frag_rel));
}

Ref<Texture> AssetManager::GetTexture(const std::string &name_or_path) {
  if (texture_library_->Exists(name_or_path)) {
    return texture_library_->Get(name_or_path);
  }

  std::string relative = name_or_path;
  const auto  it       = texture_manifest_.find(name_or_path);
  if (it != texture_manifest_.end()) {
    relative = it->second;
  }

  return texture_library_->Load(name_or_path, Resolve(relative));
}

Ref<Shader> AssetManager::GetDefaultShader() {
  if (!default_shader_) {
    default_shader_ = GetShader("default");
  }
  return default_shader_;
}

Ref<Texture> AssetManager::GetDefaultTexture() {
  if (!default_texture_) {
    default_texture_ = CreateRef<Texture>();
    unsigned char white[4] = {255, 255, 255, 255};
    default_texture_->SetData(white, 1, 1);
  }
  return default_texture_;
}

}  // namespace MEngine
