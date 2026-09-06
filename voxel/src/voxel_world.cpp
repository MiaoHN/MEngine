#include "voxel_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vox {

namespace {

using MEngine::Vertex;

/// @brief Deterministic integer hash -> [0,1).
float Hash01(int x, int y, unsigned int seed) {
  uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + seed * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return static_cast<float>(h & 0xFFFF) / 65535.0f;
}

/// @brief 2D value noise over an integer lattice (works for negative coords).
float ValueNoise(float x, float z, unsigned int seed) {
  const int ix = static_cast<int>(std::floor(x));
  const int iz = static_cast<int>(std::floor(z));
  const float fx = x - ix;
  const float fz = z - iz;
  const float u = fx * fx * (3.0f - 2.0f * fx);
  const float v = fz * fz * (3.0f - 2.0f * fz);

  const float a = Hash01(ix, iz, seed);
  const float b = Hash01(ix + 1, iz, seed);
  const float c = Hash01(ix, iz + 1, seed);
  const float d = Hash01(ix + 1, iz + 1, seed);
  return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

/// @brief Two octaves of value noise -> rolling, never-tiling hills.
float TerrainNoise(float x, float z, unsigned int seed) {
  const float scale = 0.02f;
  const float big   = ValueNoise(x * scale, z * scale, seed);
  const float mid   = ValueNoise(x * scale * 3.0f + 97.0f, z * scale * 3.0f + 31.0f, seed + 1u);
  return big * 0.72f + mid * 0.28f;
}

/// @brief A second independent hash: whether this column grows a tree.
bool WantsTree(int x, int z, unsigned int seed) {
  uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(z) * 668265263u;
  h += (seed + 0x9E3779B9u) * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return ((h & 0xFFFF) % 10000u) < 90u;  // ~0.9%
}

/// @brief Surface layer for a column of height `h` at local y.
Block LayerFor(int y, int h) {
  if (y == h - 1) {
    return (h <= 8) ? Block::Sand : Block::Grass;
  }
  if (y >= h - 4) {
    return (h <= 8) ? Block::Sand : Block::Dirt;
  }
  return Block::Stone;
}

}  // namespace

World::World(unsigned int seed) : seed_(seed) {}

void World::EnsureChunk(int cx, int cz) {
  const ChunkKey key = Key(cx, cz);
  if (chunks_.find(key) != chunks_.end()) {
    return;
  }
  auto chunk = std::vector<uint8_t>(static_cast<size_t>(kChunk) * kChunk * kHeight, 0u);
  GenerateChunk(cx, cz, chunk);
  chunks_.emplace(key, std::move(chunk));
}

bool World::HasChunk(int cx, int cz) const { return chunks_.find(Key(cx, cz)) != chunks_.end(); }

Block World::GetLocal(int cx, int cz, int lx, int ly, int lz) const {
  const auto it = chunks_.find(Key(cx, cz));
  if (it == chunks_.end()) {
    return Block::Air;
  }
  const size_t idx = static_cast<size_t>(lx) + static_cast<size_t>(kChunk) *
                                                    (static_cast<size_t>(lz) +
                                                     static_cast<size_t>(kChunk) * static_cast<size_t>(ly));
  return static_cast<Block>(it->second[idx]);
}

void World::PutLocal(int cx, int cz, int lx, int ly, int lz, Block block) {
  const auto it = chunks_.find(Key(cx, cz));
  if (it == chunks_.end()) {
    return;
  }
  const size_t idx = static_cast<size_t>(lx) + static_cast<size_t>(kChunk) *
                                                    (static_cast<size_t>(lz) +
                                                     static_cast<size_t>(kChunk) * static_cast<size_t>(ly));
  it->second[idx] = static_cast<uint8_t>(block);
}

Block World::Get(int x, int y, int z) const {
  if (y < 0 || y >= kHeight) {
    return Block::Air;
  }
  return GetLocal(ChunkCoord(x), ChunkCoord(z), LocalCoord(x), y, LocalCoord(z));
}

void World::Set(int x, int y, int z, Block block) {
  if (y < 0 || y >= kHeight) {
    return;
  }
  const int cx = ChunkCoord(x);
  const int cz = ChunkCoord(z);
  EnsureChunk(cx, cz);
  PutLocal(cx, cz, LocalCoord(x), y, LocalCoord(z), block);
}

int World::SurfaceY(int x, int z) {
  const int cx = ChunkCoord(x);
  const int cz = ChunkCoord(z);
  EnsureChunk(cx, cz);
  const int lx = LocalCoord(x);
  const int lz = LocalCoord(z);
  for (int y = kHeight - 1; y >= 0; --y) {
    if (IsSolid(GetLocal(cx, cz, lx, y, lz))) {
      return y;
    }
  }
  return -1;
}

void World::GenerateChunk(int cx, int cz, std::vector<uint8_t> &out) const {
  const int wx0 = cx * kChunk;
  const int wz0 = cz * kChunk;
  const auto put = [&](int lx, int y, int lz, Block b) {
    out[static_cast<size_t>(lx) + static_cast<size_t>(kChunk) *
                                      (static_cast<size_t>(lz) +
                                       static_cast<size_t>(kChunk) * static_cast<size_t>(y))] =
        static_cast<uint8_t>(b);
  };

  // 1) Terrain heightmap for every column of this chunk.
  std::vector<int> top(kChunk * kChunk, 0);
  for (int lx = 0; lx < kChunk; ++lx) {
    for (int lz = 0; lz < kChunk; ++lz) {
      const int wx = wx0 + lx;
      const int wz = wz0 + lz;
      float     n  = TerrainNoise(static_cast<float>(wx), static_cast<float>(wz), seed_);
      n            = n * 0.5f + 0.5f;
      const int h  = 4 + static_cast<int>(n * 28.0f);
      const int clamped = std::min(h, kHeight - 9);
      top[static_cast<size_t>(lx) * kChunk + static_cast<size_t>(lz)] = clamped;
      for (int y = 0; y < clamped; ++y) {
        put(lx, y, lz, LayerFor(y, clamped));
      }
    }
  }

  // 2) Trees on grass columns that live fully inside this chunk (so canopy and
  //    trunk never spill into a neighbour we haven't generated yet).
  for (int lx = 2; lx < kChunk - 2; ++lx) {
    for (int lz = 2; lz < kChunk - 2; ++lz) {
      const int wx = wx0 + lx;
      const int wz = wz0 + lz;
      if (!WantsTree(wx, wz, seed_)) {
        continue;
      }
      const int h = top[static_cast<size_t>(lx) * kChunk + static_cast<size_t>(lz)];
      if (h < 4 || h + 9 >= kHeight) {
        continue;
      }
      // Column top is grass only when the surface is the grass layer.
      if (GetLocal(cx, cz, lx, h - 1, lz) != Block::Grass) {
        continue;
      }
      const int trunkY = 3 + static_cast<int>(ValueNoise(static_cast<float>(wx), static_cast<float>(wz),
                                                         seed_ + 9u) *
                                   3.0f);  // 3..5
      for (int i = 0; i < trunkY; ++i) {
        put(lx, h + i, lz, Block::Wood);
      }
      const int topY = h + trunkY - 1;
      for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
          if ((dx == 0 && dz == 0) || (std::abs(dx) == 2 && std::abs(dz) == 2)) {
            continue;
          }
          put(lx + dx, topY + 1, lz + dz, Block::Leaves);
          if (std::abs(dx) <= 1 && std::abs(dz) <= 1) {
            put(lx + dx, topY + 2, lz + dz, Block::Leaves);
          }
        }
      }
      for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
          put(lx + dx, topY + 3, lz + dz, Block::Leaves);
        }
      }
      put(lx, topY + 4, lz, Block::Leaves);
    }
  }
}

// --- Meshing ----------------------------------------------------------------

void PrepareChunk(World &world, int cx, int cz) {
  world.EnsureChunk(cx, cz);
  world.EnsureChunk(cx - 1, cz);
  world.EnsureChunk(cx + 1, cz);
  world.EnsureChunk(cx, cz - 1);
  world.EnsureChunk(cx, cz + 1);
}

namespace {

// Per-face definition: outward normal + the four corners relative to the block
// MIN corner, wound CCW (same as the engine's cube) so back-face culling works.
struct FaceDef {
  glm::vec3 normal;
  glm::vec3 a, b, c, d;
  bool      top, bottom;  // atlas tile selector
};

const FaceDef kFaces[6] = {
    {{1, 0, 0}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}, false, false},  // +X
    {{-1, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}, false, false},  // -X
    {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}, true, false},    // +Y
    {{0, -1, 0}, {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}, false, true},   // -Y
    {{0, 0, 1}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}, false, false},   // +Z
    {{0, 0, -1}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}, false, false},  // -Z
};

// Neighbour step (+/- 1) tested per face index.
const int kStep[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

/// @brief Maps a block-local corner (each component 0 or 1) to a fractional
/// (u, v) so the texture is NEVER rotated on a face: v always runs up the
/// block (local Y) on side faces; top/bottom tiles are drawn flat.
glm::vec2 UprightUV(const glm::vec3 &local, const glm::vec3 &normal) {
  if (std::abs(normal.y) > 0.5f) {
    return {local.x, local.z};  // top / bottom face
  }
  if (std::abs(normal.x) > 0.5f) {
    return {local.z, local.y};  // +X / -X side: u across, v up
  }
  return {local.x, local.y};  // +Z / -Z side
}

}  // namespace

void BuildChunkMesh(const World &world, const Atlas &atlas, int chunk_x, int chunk_z,
                    std::vector<Vertex> &out_vertices, std::vector<uint32_t> &out_indices) {
  const int wx0 = chunk_x * World::kChunk;
  const int wz0 = chunk_z * World::kChunk;

  for (int lx = 0; lx < World::kChunk; ++lx) {
    for (int lz = 0; lz < World::kChunk; ++lz) {
      const int wx = wx0 + lx;
      const int wz = wz0 + lz;
      for (int y = 0; y < World::kHeight; ++y) {
        const Block block = world.Get(wx, y, wz);
        if (!IsSolid(block)) {
          continue;
        }
        for (int f = 0; f < 6; ++f) {
          if (world.IsSolidCell(wx + kStep[f][0], y + kStep[f][1], wz + kStep[f][2])) {
            continue;  // hidden face
          }
          const FaceDef &fd    = kFaces[f];
          const glm::vec3 o(static_cast<float>(wx), static_cast<float>(y), static_cast<float>(wz));
          const glm::vec3 corners[4] = {o + fd.a, o + fd.b, o + fd.c, o + fd.d};
          const glm::vec3 local[4]   = {fd.a, fd.b, fd.c, fd.d};

          const TileId tile = atlas.TileFor(block, fd.top, fd.bottom);
          const uint32_t base = static_cast<uint32_t>(out_vertices.size());
          for (int i = 0; i < 4; ++i) {
            const glm::vec2 uv = atlas.TileUV(tile, UprightUV(local[i], fd.normal).x,
                                              UprightUV(local[i], fd.normal).y);
            out_vertices.push_back({corners[i], fd.normal, uv});
          }
          out_indices.push_back(base + 0);
          out_indices.push_back(base + 1);
          out_indices.push_back(base + 2);
          out_indices.push_back(base + 2);
          out_indices.push_back(base + 3);
          out_indices.push_back(base + 0);
        }
      }
    }
  }
}

}  // namespace vox
