/**
 * @file voxel_world.hpp
 * @brief Infinite, procedurally generated chunked voxel world for the demo.
 *
 * The world is a map of 16x16x[kHeight] chunks that are generated lazily and
 * deterministically from a seed (value-noise heightmap + trees). This is what
 * makes the terrain "generated and unlimited" instead of a fixed pre-built box.
 * Engine types are only used at the meshing boundary (Vertex).
 */

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "render/vertex.hpp"

#include "voxel_atlas.hpp"

namespace vox {

/// @brief Lazily-generated, endless voxel world (horizontal XZ plane).
class World {
 public:
  static constexpr int kChunk  = 16;
  static constexpr int kHeight = 40;
  static constexpr int kSeaLevel = 16;  // water surface occupies y = kSeaLevel-1

  explicit World(unsigned int seed = 1337u);
  ~World() = default;

  [[nodiscard]] unsigned int Seed() const { return seed_; }

  /// @brief Chunk column that owns world coordinate `x`.
  static int ChunkCoord(int x) { return (x >= 0) ? x / kChunk : -((-x + kChunk - 1) / kChunk); }
  static int LocalCoord(int x) { return x - ChunkCoord(x) * kChunk; }

  /// @brief Ensures the chunk (cx, cz) is generated.
  void EnsureChunk(int cx, int cz);
  /// @brief True when the chunk has been generated already.
  [[nodiscard]] bool HasChunk(int cx, int cz) const;

  /// @brief Block at world coords (Air for y outside [0, kHeight) or a not-yet
  /// generated chunk — callers ensure chunks are generated before reading).
  [[nodiscard]] Block Get(int x, int y, int z) const;

  /// @brief Sets a block (ignored when y is out of vertical range).
  void Set(int x, int y, int z, Block block);

  /// @brief Highest solid y in column (x, z); -1 for an empty column. The
  /// owning chunk is generated on demand. Ignores water / leaves / wood so it
  /// returns actual terrain for spawning.
  int SurfaceY(int x, int z);

  /// @brief Opaque-block test (water & leaves included) used by the mesher.
  [[nodiscard]] bool IsSolidCell(int x, int y, int z) const { return IsOpaque(Get(x, y, z)); }
  /// @brief Collidable-block test (water is passable) used by the player.
  [[nodiscard]] bool IsSolidCollision(int x, int y, int z) const { return IsCollidable(Get(x, y, z)); }
  /// @brief Meshing occlusion test: opaque solids only, water is translucent
  /// and never hides a neighbour's face (coastline terrain stays visible).
  [[nodiscard]] bool IsOccluding(int x, int y, int z) const { return IsOccluder(Get(x, y, z)); }

 private:
  using ChunkKey = int64_t;
  static ChunkKey Key(int cx, int cz) {
    return (static_cast<ChunkKey>(cx) << 32) ^ static_cast<ChunkKey>(static_cast<uint32_t>(cz));
  }

  /// @brief Deterministically fills `out` (kChunk*kChunk*kHeight bytes) for the
  /// given chunk column.
  void GenerateChunk(int cx, int cz, std::vector<uint8_t> &out) const;

  /// @brief Writes a block into chunk `cx,cz`'s local volume (assumes present).
  void PutLocal(int cx, int cz, int lx, int ly, int lz, Block block);
  /// @brief Reads from a chunk that must already be generated.
  [[nodiscard]] Block GetLocal(int cx, int cz, int lx, int ly, int lz) const;

  unsigned int seed_;
  std::unordered_map<ChunkKey, std::vector<uint8_t>> chunks_;
};

/// @brief Generates the chunk (and its four neighbours) so meshing can cull
/// faces correctly across borders.
void PrepareChunk(World &world, int cx, int cz);

/// @brief Builds the visible mesh for the chunk at (chunk_x, chunk_z) into
/// world-space vertices. `world` chunks for this column + neighbours must be
/// generated first (see PrepareChunk).
///
/// Opaque blocks land in `out_vertices`/`out_indices`; translucent water
/// surfaces land in `water_vertices`/`water_indices` so the caller can give
/// them a separate alpha-blended material.
void BuildChunkMesh(const World &world, const Atlas &atlas, int chunk_x, int chunk_z,
                    std::vector<MEngine::Vertex> &out_vertices, std::vector<uint32_t> &out_indices,
                    std::vector<MEngine::Vertex> &water_vertices, std::vector<uint32_t> &water_indices);

}  // namespace vox
