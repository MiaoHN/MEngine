#include "voxel_atlas.hpp"

#include <algorithm>
#include <cmath>

namespace vox {

namespace {

/// @brief Cheap deterministic integer hash -> [0,1).
float HashNoise(int x, int y) {
  uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + 1540483477u;
  h          = (h ^ (h >> 13)) * 1274126177u;
  h          = h ^ (h >> 16);
  return static_cast<float>(h & 0xFFFF) / 65535.0f;
}

uint8_t f(float v) { return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f); }

void PutPixel(std::vector<uint8_t> &rgba, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  const size_t o = (static_cast<size_t>(y) * Atlas::kWidth + static_cast<size_t>(x)) * 4u;
  rgba[o + 0]    = r;
  rgba[o + 1]    = g;
  rgba[o + 2]    = b;
  rgba[o + 3]    = 255;
}

/// @brief Texture value (RGB triples) for each tile type at content pixel
/// (px, rowFromBottom) in [0,16). Fills grass-side / wood / plank variants.
void Sample(TileId tile, int px, int py /*0=bottom*/, float &r, float &g, float &b) {
  const float n = HashNoise(px * 7 + 1, py * 13 + py * py) * 0.5f + 0.5f;
  const auto  shade = [n](float base, float spread = 0.12f) { return base * (1.0f - spread + spread * n); };
  switch (tile) {
    case TileGrassTop:
      r = shade(0.44f);
      g = shade(0.76f);
      b = shade(0.30f);
      break;
    case TileGrassSide: {  // dirt body with a green top strip
      if (py >= 12) {
        r = shade(0.44f);
        g = shade(0.76f);
        b = shade(0.30f);
      } else {
        r = shade(0.62f);
        g = shade(0.42f);
        b = shade(0.25f);
      }
      break;
    }
    case TileDirt:
      r = shade(0.62f);
      g = shade(0.43f);
      b = shade(0.26f);
      break;
    case TileStone:
      r = shade(0.55f, 0.25f);
      g = shade(0.55f, 0.25f);
      b = shade(0.58f, 0.25f);
      break;
    case TileSand:
      r = shade(0.85f);
      g = shade(0.78f);
      b = shade(0.52f);
      break;
    case TileWoodSide: {  // vertical bark stripes
      const float stripe = (px % 4 == 0 || px % 4 == 3) ? 0.72f : 1.0f;
      r = shade(0.45f * stripe);
      g = shade(0.30f * stripe);
      b = shade(0.16f * stripe);
      break;
    }
    case TileWoodTop: {  // concentric-ish rings by distance from tile centre
      const float dx = (px + 0.5f) - 8.0f;
      const float dy = (py + 0.5f) - 8.0f;
      const float d  = std::sqrt(dx * dx + dy * dy);
      const float ring = (static_cast<int>(d) % 3 == 0) ? 0.72f : 1.0f;
      r = shade(0.62f * ring);
      g = shade(0.46f * ring);
      b = shade(0.26f * ring);
      break;
    }
    case TileLeaves:
      r = shade(0.26f, 0.25f);
      g = shade(0.52f, 0.25f);
      b = shade(0.18f, 0.25f);
      break;
    default:
      r = 1.0f;
      g = 0.0f;
      b = 1.0f;  // magenta = missing
      break;
  }
}

}  // namespace

Atlas::Atlas() : rgba_(static_cast<size_t>(kWidth) * kHeight * 4u, 0) {
  // Draw every tile content into its 16x16 region.
  for (int t = 0; t < TileCount; ++t) {
    const int col = t % kCols;
    const int row = t / kCols;
    const int x0  = col * kStride + 1;
    const int y0  = row * kStride + 1;  // bottom of the tile's content

    // bottom-up scratch so borders can be replicated afterwards.
    float r[16][16], g[16][16], b[16][16];
    for (int px = 0; px < 16; ++px) {
      for (int py = 0; py < 16; ++py) {
        Sample(static_cast<TileId>(t), px, py, r[py][px], g[py][px], b[py][px]);
        PutPixel(rgba_, x0 + px, y0 + py, f(r[py][px]), f(g[py][px]), f(b[py][px]));
      }
    }
    // Replicate a 1 px border so linear filtering never bleeds a neighbour in.
    for (int px = 0; px < 16; ++px) {
      PutPixel(rgba_, x0 + px, y0 - 1, f(r[0][px]), f(g[0][px]), f(b[0][px]));
      PutPixel(rgba_, x0 + px, y0 + 16, f(r[15][px]), f(g[15][px]), f(b[15][px]));
    }
    for (int py = 0; py < 16; ++py) {
      PutPixel(rgba_, x0 - 1, y0 + py, f(r[py][0]), f(g[py][0]), f(b[py][0]));
      PutPixel(rgba_, x0 + 16, y0 + py, f(r[py][15]), f(g[py][15]), f(b[py][15]));
    }
    PutPixel(rgba_, x0 - 1, y0 - 1, f(r[0][0]), f(g[0][0]), f(b[0][0]));
    PutPixel(rgba_, x0 + 16, y0 - 1, f(r[0][15]), f(g[0][15]), f(b[0][15]));
    PutPixel(rgba_, x0 - 1, y0 + 16, f(r[15][0]), f(g[15][0]), f(b[15][0]));
    PutPixel(rgba_, x0 + 16, y0 + 16, f(r[15][15]), f(g[15][15]), f(b[15][15]));
  }
}

TileId Atlas::TileFor(Block block, bool top, bool bottom) const {
  switch (block) {
    case Block::Grass: return top ? TileGrassTop : (bottom ? TileDirt : TileGrassSide);
    case Block::Dirt: return TileDirt;
    case Block::Stone: return TileStone;
    case Block::Sand: return TileSand;
    case Block::Wood: return (top || bottom) ? TileWoodTop : TileWoodSide;
    case Block::Leaves: return TileLeaves;
    default: return TileStone;
  }
}

glm::vec2 Atlas::TileUV(TileId tile, float u, float v) const {
  const int col = static_cast<int>(tile) % kCols;
  const int row = static_cast<int>(tile) / kCols;
  // Content region is [x0, x0+16] x [y0, y0+16]; leave the 1px border out.
  const float u0 = static_cast<float>(col * kStride + 1) / kWidth;
  const float u1 = static_cast<float>(col * kStride + kStride - 1) / kWidth;
  const float v0 = static_cast<float>(row * kStride + 1) / kHeight;
  const float v1 = static_cast<float>(row * kStride + kStride - 1) / kHeight;
  return {u0 + (u1 - u0) * u, v0 + (v1 - v0) * v};
}

}  // namespace vox
