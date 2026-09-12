#include "render/sprite.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "render/asset_manager.hpp"
#include "render/shader.hpp"
#include "render/vertex.hpp"

namespace MEngine {

namespace {

/// @brief Cache key of one quad variant: the UV rectangle (quantized to 1e-4 so
/// float noise from a sheet calculation cannot create duplicate meshes) plus the
/// two flip flags.
struct QuadKey {
  int32_t uv[4] = {0, 0, 0, 0};
  uint8_t flips = 0;

  bool operator==(const QuadKey &other) const {
    return flips == other.flips && std::memcmp(uv, other.uv, sizeof(uv)) == 0;
  }
};

struct QuadKeyHash {
  size_t operator()(const QuadKey &key) const {
    size_t h = 1469598103934665603ull;
    for (const int32_t v : key.uv) {
      h = (h ^ static_cast<size_t>(static_cast<uint32_t>(v))) * 1099511628211ull;
    }
    return (h ^ key.flips) * 1099511628211ull;
  }
};

int32_t Quantize(float value) { return static_cast<int32_t>(std::lround(value * 10000.0f)); }

/// @brief Quad cache; sprites live on the render thread, so no locking.
std::unordered_map<QuadKey, Ref<Mesh>, QuadKeyHash> &QuadCache() {
  static std::unordered_map<QuadKey, Ref<Mesh>, QuadKeyHash> cache;
  return cache;
}

}  // namespace

glm::vec4 SpriteSheet::FrameRect(int frame) const {
  const int column_count = std::max(1, columns);
  const int row_count    = std::max(1, rows);
  const int frame_count  = column_count * row_count;
  int       index        = frame % frame_count;
  if (index < 0) {
    index += frame_count;
  }
  const int column = index % column_count;
  const int row    = index / column_count;  // 0 = top row of the image

  const float u0 = static_cast<float>(column) / static_cast<float>(column_count);
  const float u1 = static_cast<float>(column + 1) / static_cast<float>(column_count);
  // v runs bottom-up, so the top row of the sheet has v in [1 - 1/rows, 1].
  const float v1 = 1.0f - static_cast<float>(row) / static_cast<float>(row_count);
  const float v0 = 1.0f - static_cast<float>(row + 1) / static_cast<float>(row_count);
  return {u0, v0, u1, v1};
}

Ref<Mesh> GetSpriteQuad(const glm::vec4 &uv_rect, bool flip_x, bool flip_y) {
  QuadKey key;
  key.uv[0] = Quantize(uv_rect.x);
  key.uv[1] = Quantize(uv_rect.y);
  key.uv[2] = Quantize(uv_rect.z);
  key.uv[3] = Quantize(uv_rect.w);
  key.flips = static_cast<uint8_t>((flip_x ? 1u : 0u) | (flip_y ? 2u : 0u));

  auto &cache = QuadCache();
  if (const auto it = cache.find(key); it != cache.end()) {
    return it->second;
  }

  // Unit quad centered on the origin, facing +Z, wound CCW seen from +Z.
  const float u0 = flip_x ? uv_rect.z : uv_rect.x;
  const float u1 = flip_x ? uv_rect.x : uv_rect.z;
  const float v0 = flip_y ? uv_rect.w : uv_rect.y;
  const float v1 = flip_y ? uv_rect.y : uv_rect.w;

  const std::vector<Vertex> vertices = {
      {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {u0, v0}},
      {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {u1, v0}},
      {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {u1, v1}},
      {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {u0, v1}},
  };
  const std::vector<unsigned int> indices = {0, 1, 2, 2, 3, 0};

  Ref<Mesh> quad = Mesh::Create(vertices, indices);
  cache.emplace(key, quad);
  return quad;
}

Ref<Material> CreateSpriteMaterial(const Ref<Texture> &texture, const glm::vec4 &color) {
  auto material = CreateRef<Material>();
  // Dedicated 2D shader: unlit, no lighting/shadow/IBL uniforms, no tone
  // mapping - see assets/shaders/sprite_{vert,frag}.glsl.
  material->SetShader(AssetManager::Instance().GetShader("sprite"));
  material->SetAlbedoMap(texture);
  material->SetBaseColorFactor(color);
  material->SetMetallicFactor(0.0f);
  material->SetRoughnessFactor(1.0f);
  material->SetUnlit(true);        // also keeps the 3D pass from lighting it
  material->SetTranslucent(true);  // alpha blended, no depth write when drawn in 3D
  material->SetCullMode(CullMode::None);  // sprites are flat and double-sided
  return material;
}

}  // namespace MEngine
