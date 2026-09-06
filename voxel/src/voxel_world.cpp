#include "voxel_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vox {

namespace {

using MEngine::Vertex;

/// @brief 2D value noise over an integer lattice (smoothstep interpolation).
float ValueNoise(float x, float z, unsigned int seed) {
  const int ix = static_cast<int>(std::floor(x));
  const int iz = static_cast<int>(std::floor(z));
  const float fx = x - ix;
  const float fz = z - iz;

  auto hash = [seed](int cx, int cz) {
    uint32_t h = static_cast<uint32_t>(cx) * 374761393u + static_cast<uint32_t>(cz) * 668265263u;
    h += seed * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFF) / 65535.0f;
  };

  const float u = fx * fx * (3.0f - 2.0f * fx);
  const float v = fz * fz * (3.0f - 2.0f * fz);

  const float a = hash(ix, iz);
  const float b = hash(ix + 1, iz);
  const float c = hash(ix, iz + 1);
  const float d = hash(ix + 1, iz + 1);

  return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

/// @brief A couple of summed-noise octaves for rolling hills.
float TerrainNoise(float x, float z, unsigned int seed) {
  const float scale = 0.018f;
  const float big   = ValueNoise(x * scale, z * scale, seed);
  const float mid   = ValueNoise(x * scale * 3.0f + 100.0f, z * scale * 3.0f + 100.0f, seed + 1u);
  return big * 0.7f + mid * 0.3f;
}

/// @brief Per-column deterministic tree decision (~1.2% of columns).
bool WantsTree(int x, int z, unsigned int seed) {
  uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(z) * 668265263u;
  h += seed * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return ((h & 0xFFFF) % 10000u) < 120u;  // ~1.2%
}

}  // namespace

World::World(int chunk_count, int height, unsigned int seed)
    : chunk_count_(chunk_count),
      height_(height),
      seed_(seed),
      blocks_(static_cast<size_t>(SizeXZ()) * SizeXZ() * height_, 0) {
  Generate();
}

Block World::Get(int x, int y, int z) const {
  const int xz = SizeXZ();
  if (x < 0 || z < 0 || y < 0 || x >= xz || z >= xz || y >= height_) {
    return Block::Air;  // edges are open space
  }
  return static_cast<Block>(blocks_[static_cast<size_t>(x) + static_cast<size_t>(xz) * (static_cast<size_t>(z) +
                                                                                      static_cast<size_t>(xz) * y)]);
}

void World::Set(int x, int y, int z, Block block) {
  const int xz = SizeXZ();
  if (x < 0 || z < 0 || y < 0 || x >= xz || z >= xz || y >= height_) {
    return;
  }
  blocks_[static_cast<size_t>(x) + static_cast<size_t>(xz) * (static_cast<size_t>(z) +
                                                              static_cast<size_t>(xz) * y)] =
      static_cast<uint8_t>(block);
}

int World::SurfaceY(int x, int z) const {
  for (int y = height_ - 1; y >= 0; --y) {
    if (IsSolidCell(x, y, z)) {
      return y;
    }
  }
  return -1;
}

void World::PlantTree(int x, int y, int z) {
  const int trunk = 4 + static_cast<int>(ValueNoise(static_cast<float>(x), static_cast<float>(z), seed_ + 9u) * 3.0f);
  if (y + trunk + 3 >= height_) {
    return;
  }
  for (int i = 0; i < trunk; ++i) {
    Set(x, y + i, z, Block::Wood);
  }
  const int top = y + trunk - 1;  // highest wood block
  // Oak-ish canopy: wide base layer (5x5), narrower 3x3 above, single cap.
  for (int dx = -2; dx <= 2; ++dx) {
    for (int dz = -2; dz <= 2; ++dz) {
      if (std::abs(dx) == 2 && std::abs(dz) == 2) {
        continue;  // trim the corners for a rounder blob
      }
      if (dx == 0 && dz == 0) {
        continue;  // the trunk itself already fills this
      }
      Set(x + dx, top + 1, z + dz, Block::Leaves);
      if (std::abs(dx) <= 1 && std::abs(dz) <= 1) {
        Set(x + dx, top + 2, z + dz, Block::Leaves);
      }
    }
  }
  // cap above the trunk
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dz = -1; dz <= 1; ++dz) {
      Set(x + dx, top + 3, z + dz, Block::Leaves);
    }
  }
  Set(x, top + 4, z, Block::Leaves);
}

void World::Generate() {
  const int xz  = SizeXZ();
  const int cyc = kChunk;

  // 1) Heightmap + terrain layers.
  for (int x = 0; x < xz; ++x) {
    for (int z = 0; z < xz; ++z) {
      float n = TerrainNoise(static_cast<float>(x), static_cast<float>(z), seed_);
      n       = n * 0.5f + 0.5f;
      // Map to a nice hill band (y roughly 6..30).
      const int h = 6 + static_cast<int>(n * 24.0f);
      const int clamped = std::min(h, height_ - 8);
      for (int y = 0; y < clamped; ++y) {
        Block b = Block::Stone;
        if (y == clamped - 1) {
          b = (clamped <= 9) ? Block::Sand : Block::Grass;
        } else if (y >= clamped - 4) {
          b = Block::Dirt;
        } else if (clamped <= 9 && y >= clamped - 3) {
          b = Block::Sand;
        }
        Set(x, y, z, b);
      }
    }
  }

  // 2) Trees on grass (deterministic); keep the spawn column clear.
  const int spawn = xz / 2;
  for (int x = 2; x < xz - 2; x += 1) {
    for (int z = 2; z < xz - 2; z += 1) {
      if (!WantsTree(x, z, seed_)) {
        continue;
      }
      const int dx = x - spawn;
      const int dz = z - spawn;
      if (dx * dx + dz * dz < 12 * 12) {
        continue;  // no trees right at spawn
      }
      const int y = SurfaceY(x, z);
      if (y >= 0 && Get(x, y, z) == Block::Grass) {
        PlantTree(x, y + 1, z);
      }
    }
  }

  (void)cyc;
}

namespace {
// Replicates the engine cube's per-face winding so generated quads render with
// back-face culling on. Face -> (normal, four offsets from the block MIN corner,
// where the 4 offsets map to engine UV corners a(0,0) b(1,0) c(1,1) d(0,1)).
struct FaceDef {
  glm::vec3 normal;
  glm::vec3 a, b, c, d;  // relative to block min corner
  int corner_u[4];       // engine corner u (0/1) per vertex
  int corner_v[4];       // engine corner v (0/1) per vertex
  bool top, bottom;      // which atlas tile to use
};

const FaceDef kFaces[6] = {
    // +X
    {{1, 0, 0}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}, {0, 1, 1, 0}, {0, 0, 1, 1}, false, false},
    // -X
    {{-1, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 1}, false, false},
    // +Y (top)
    {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}, {0, 1, 1, 0}, {0, 0, 1, 1}, true, false},
    // -Y (bottom)
    {{0, -1, 0}, {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}, {0, 1, 1, 0}, {0, 0, 1, 1}, false, true},
    // +Z
    {{0, 0, 1}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {0, 1, 1, 0}, {0, 0, 1, 1}, false, false},
    // -Z
    {{0, 0, -1}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {0, 1, 1, 0}, {0, 0, 1, 1}, false, false},
};

// Which +1/-1 neighbour to test for each face.
const int kStep[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

}  // namespace

void BuildChunkMesh(const World &world, const Atlas &atlas, int chunk_x, int chunk_z,
                    std::vector<Vertex> &out_vertices, std::vector<uint32_t> &out_indices) {
  const int ox = chunk_x * World::kChunk;
  const int oz = chunk_z * World::kChunk;

  const size_t first_index = out_vertices.size();
  (void)first_index;

  for (int lx = 0; lx < World::kChunk; ++lx) {
    for (int lz = 0; lz < World::kChunk; ++lz) {
      const int wx = ox + lx;
      const int wz = oz + lz;
      for (int y = 0; y < world.Height(); ++y) {
        const Block block = world.Get(wx, y, wz);
        if (!IsSolid(block)) {
          continue;
        }
        for (int f = 0; f < 6; ++f) {
          const int nx = wx + kStep[f][0];
          const int ny = y + kStep[f][1];
          const int nz = wz + kStep[f][2];
          if (world.IsSolidCell(nx, ny, nz)) {
            continue;  // hidden face
          }
          const FaceDef &fd = kFaces[f];
          const TileId tile = atlas.TileFor(block, fd.top, fd.bottom);
          const glm::vec3 o(static_cast<float>(wx), static_cast<float>(y), static_cast<float>(wz));
          const glm::vec3 corners[4] = {o + fd.a, o + fd.b, o + fd.c, o + fd.d};
          const uint32_t base       = static_cast<uint32_t>(out_vertices.size());
          for (int i = 0; i < 4; ++i) {
            const glm::vec2 uv = atlas.TileUV(tile, fd.corner_u[i], fd.corner_v[i]);
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
