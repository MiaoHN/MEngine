// glTF 2.0 loader built on tinygltf.
//
// TINYGLTF_IMPLEMENTATION is defined exactly once, here. The stb_image
// implementation lives in texture.cpp; tinygltf only calls the stbi_* API.
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

// tiny_gltf.h includes <windows.h> on Windows, which #defines ERROR and would
// break the Logger::Level::ERROR enum below. Undef it like core/common.hpp
// does after including GLFW.
#ifdef ERROR
#undef ERROR
#endif

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "core/logger.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/model_loader.hpp"
#include "render/texture.hpp"

namespace MEngine {

namespace {

// Reads an accessor's elements into a vector<T>.
//
// Handles interleaved vertex data via Accessor::ByteStride. Assumes the
// component type / layout of T already matches the accessor (validated by the
// callers below).
template <typename T>
std::vector<T> ReadAccessor(const tinygltf::Model &model, int accessor_index) {
  const tinygltf::Accessor &acc = model.accessors[accessor_index];
  const tinygltf::BufferView &bv = model.bufferViews[acc.bufferView];
  const tinygltf::Buffer &buf = model.buffers[bv.buffer];

  const size_t stride = static_cast<size_t>(acc.ByteStride(bv));
  const unsigned char *base = buf.data.data() + bv.byteOffset + acc.byteOffset;

  std::vector<T> out(acc.count);
  for (size_t i = 0; i < acc.count; ++i) {
    std::memcpy(&out[i], base + i * stride, sizeof(T));
  }
  return out;
}

std::vector<unsigned int> ReadIndices(const tinygltf::Model &model, int accessor_index) {
  const tinygltf::Accessor &acc = model.accessors[accessor_index];
  std::vector<unsigned int> indices(acc.count);
  switch (acc.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
      const auto data = ReadAccessor<unsigned char>(model, accessor_index);
      for (size_t i = 0; i < data.size(); ++i) {
        indices[i] = data[i];
      }
      break;
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
      const auto data = ReadAccessor<unsigned short>(model, accessor_index);
      for (size_t i = 0; i < data.size(); ++i) {
        indices[i] = data[i];
      }
      break;
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
      const auto data = ReadAccessor<unsigned int>(model, accessor_index);
      for (size_t i = 0; i < data.size(); ++i) {
        indices[i] = data[i];
      }
      break;
    }
    default:
      LOG_ERROR("ModelLoader") << "Unsupported glTF index component type: " << acc.componentType;
      return {};
  }
  return indices;
}

bool LoadFile(tinygltf::Model &model, const std::string &path, std::string &err, std::string &warn) {
  tinygltf::TinyGLTF loader;
  if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".glb") == 0) {
    return loader.LoadBinaryFromFile(&model, &err, &warn, path);
  }
  return loader.LoadASCIIFromFile(&model, &err, &warn, path);
}

// Converts a decoded tinygltf image to tightly packed RGBA8.
std::vector<unsigned char> ToRGBA(const tinygltf::Image &img) {
  const size_t pixel_count = static_cast<size_t>(img.width) * static_cast<size_t>(img.height);
  std::vector<unsigned char> rgba(pixel_count * 4);
  for (size_t i = 0; i < pixel_count; ++i) {
    unsigned char r = 0, g = 0, b = 0, a = 255;
    switch (img.component) {
      case 1:
        r = g = b = img.image[i];
        break;
      case 2:
        r = g = b = img.image[i * 2];
        a = img.image[i * 2 + 1];
        break;
      case 3:
        r = img.image[i * 3];
        g = img.image[i * 3 + 1];
        b = img.image[i * 3 + 2];
        break;
      case 4:
        r = img.image[i * 4];
        g = img.image[i * 4 + 1];
        b = img.image[i * 4 + 2];
        a = img.image[i * 4 + 3];
        break;
      default:
        break;
    }
    rgba[i * 4]     = r;
    rgba[i * 4 + 1] = g;
    rgba[i * 4 + 2] = b;
    rgba[i * 4 + 3] = a;
  }
  return rgba;
}

// Decodes and uploads the glTF texture at `texture_index` to a Texture.
Ref<Texture> LoadGltfTexture(const tinygltf::Model &model, int texture_index) {
  if (texture_index < 0 || texture_index >= static_cast<int>(model.textures.size())) {
    return nullptr;
  }
  const int image_index = model.textures[texture_index].source;
  if (image_index < 0 || image_index >= static_cast<int>(model.images.size())) {
    return nullptr;
  }
  const tinygltf::Image &image = model.images[image_index];
  if (image.image.empty() || image.width <= 0 || image.height <= 0) {
    return nullptr;
  }

  std::vector<unsigned char> rgba    = ToRGBA(image);
  auto                       texture = CreateRef<Texture>();
  texture->SetData(rgba.data(), image.width, image.height);
  return texture;
}

}  // namespace

Ref<Mesh> ModelLoader::LoadGltf(const std::string &path) {
  tinygltf::Model model;
  std::string     err, warn;
  if (!LoadFile(model, path, err, warn)) {
    LOG_ERROR("ModelLoader") << "Failed to load glTF '" << path << "': " << err;
    return nullptr;
  }
  if (!warn.empty()) {
    LOG_WARN("ModelLoader") << "glTF warnings for '" << path << "': " << warn;
  }

  if (model.meshes.empty() || model.meshes[0].primitives.empty()) {
    LOG_ERROR("ModelLoader") << "glTF has no meshes: " << path;
    return nullptr;
  }

  const tinygltf::Primitive &prim = model.meshes[0].primitives[0];

  // POSITION is required.
  const auto pos_it = prim.attributes.find("POSITION");
  if (pos_it == prim.attributes.end()) {
    LOG_ERROR("ModelLoader") << "glTF primitive has no POSITION attribute: " << path;
    return nullptr;
  }
  std::vector<glm::vec3> positions = ReadAccessor<glm::vec3>(model, pos_it->second);

  std::vector<glm::vec3> normals;
  const auto normal_it = prim.attributes.find("NORMAL");
  if (normal_it != prim.attributes.end()) {
    normals = ReadAccessor<glm::vec3>(model, normal_it->second);
  }

  std::vector<glm::vec2> texcoords;
  const auto tex_it = prim.attributes.find("TEXCOORD_0");
  if (tex_it != prim.attributes.end()) {
    texcoords = ReadAccessor<glm::vec2>(model, tex_it->second);
  }

  std::vector<unsigned int> indices;
  if (prim.indices >= 0) {
    indices = ReadIndices(model, prim.indices);
    if (indices.empty()) {
      LOG_ERROR("ModelLoader") << "Failed to read glTF indices: " << path;
      return nullptr;
    }
  } else {
    indices.resize(positions.size());
    for (size_t i = 0; i < indices.size(); ++i) {
      indices[i] = static_cast<unsigned int>(i);
    }
  }

  std::vector<Vertex>       vertices;
  std::vector<unsigned int> out_indices;

  if (normals.size() == positions.size()) {
    // Indexed, with per-vertex normals.
    vertices.resize(positions.size());
    for (size_t i = 0; i < positions.size(); ++i) {
      vertices[i].position = positions[i];
      vertices[i].normal   = normals[i];
      vertices[i].texcoord = (i < texcoords.size()) ? texcoords[i] : glm::vec2(0.0f);
    }
    out_indices = indices;
  } else {
    // No normals: emit flat-shaded triangles (3 fresh vertices each).
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
      const glm::vec3 &p0 = positions[indices[i]];
      const glm::vec3 &p1 = positions[indices[i + 1]];
      const glm::vec3 &p2 = positions[indices[i + 2]];
      const glm::vec3 face_normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));

      for (size_t k = 0; k < 3; ++k) {
        const unsigned int idx = indices[i + k];
        Vertex              vertex;
        vertex.position = positions[idx];
        vertex.normal   = face_normal;
        vertex.texcoord = (idx < texcoords.size()) ? texcoords[idx] : glm::vec2(0.0f);
        out_indices.push_back(static_cast<unsigned int>(vertices.size()));
        vertices.push_back(vertex);
      }
    }
  }

  LOG_INFO("ModelLoader") << "Loaded glTF '" << path << "': " << vertices.size() << " vertices, "
                          << out_indices.size() << " indices.";
  return Mesh::Create(vertices, out_indices);
}

Ref<Texture> ModelLoader::LoadGltfBaseColorTexture(const std::string &path) {
  tinygltf::Model model;
  std::string     err, warn;
  if (!LoadFile(model, path, err, warn)) {
    LOG_ERROR("ModelLoader") << "Failed to load glTF '" << path << "': " << err;
    return nullptr;
  }

  for (const tinygltf::Material &mat : model.materials) {
    const int texture_index = mat.pbrMetallicRoughness.baseColorTexture.index;
    if (auto texture = LoadGltfTexture(model, texture_index)) {
      return texture;
    }
  }

  return nullptr;
}

Ref<Material> ModelLoader::LoadGltfMaterial(const std::string &path) {
  tinygltf::Model model;
  std::string     err, warn;
  if (!LoadFile(model, path, err, warn)) {
    LOG_ERROR("ModelLoader") << "Failed to load glTF '" << path << "': " << err;
    return nullptr;
  }

  if (model.materials.empty()) {
    LOG_WARN("ModelLoader") << "glTF has no materials: " << path;
    return nullptr;
  }

  const tinygltf::Material &mat = model.materials[0];
  auto                      material = CreateRef<Material>();

  material->SetAlbedoMap(LoadGltfTexture(model, mat.pbrMetallicRoughness.baseColorTexture.index));
  material->SetMetallicRoughnessMap(
      LoadGltfTexture(model, mat.pbrMetallicRoughness.metallicRoughnessTexture.index));
  material->SetNormalMap(LoadGltfTexture(model, mat.normalTexture.index));
  material->SetAOMap(LoadGltfTexture(model, mat.occlusionTexture.index));

  if (mat.pbrMetallicRoughness.baseColorFactor.size() == 4) {
    material->SetBaseColorFactor(glm::vec4(static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[0]),
                                           static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[1]),
                                           static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[2]),
                                           static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[3])));
  }
  material->SetMetallicFactor(static_cast<float>(mat.pbrMetallicRoughness.metallicFactor));
  material->SetRoughnessFactor(static_cast<float>(mat.pbrMetallicRoughness.roughnessFactor));

  return material;
}

}  // namespace MEngine
