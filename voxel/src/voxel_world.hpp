/**
 * @file voxel_world.hpp
 * @brief Infinite, procedurally generated chunked voxel world for the demo.
 *
 * The world is a map of 16x16x[kHeight] chunks that are generated lazily and
 * deterministically from a seed (value-noise heightmap + trees). This is what
 * makes the terrain "generated and unlimited" instead of a fixed pre-built box.
 * Engine types are only used at the meshing boundary (Vertex).
 *
 * THREADING (Minecraft-style split of "chunk build" vs "chunk render"): the
 * world is shared with the background chunk streamer (`voxel_streamer.hpp`).
 * Every accessor here is safe to call from any thread; the bulk meshing path
 * never walks the map directly but reads a `ChunkSnapshot` copy, so meshing
 * holds no lock and does no hash lookup per block.
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "render/vertex.hpp"

#include "voxel_atlas.hpp"

namespace vox {

class ChunkSnapshot;

/// @brief Lazily-generated, endless voxel world (horizontal XZ plane).
class World {
 public:
  static constexpr int kChunk    = 16;
  static constexpr int kHeight   = 40;
  static constexpr int kSeaLevel = 16;  // water surface occupies y = kSeaLevel-1

  explicit World(unsigned int seed = 1337u);
  ~World() = default;

  // The chunk map is shared with worker threads: no copying / moving.
  World(const World &)            = delete;
  World &operator=(const World &) = delete;

  [[nodiscard]] unsigned int Seed() const { return seed_; }

  /// @brief Pure (no lock, no allocation, no generation): surface height of a
  /// world column. The column's terrain blocks occupy y in [0, TerrainHeight).
  [[nodiscard]] int TerrainHeight(int x, int z) const;

  /// @brief Deterministically generates one chunk column into `out`
  /// (kChunk*kChunk*kHeight bytes, layout [y][z][x]). Pure and thread-safe: it
  /// reads only the seed, so background workers may call it concurrently.
  void GenerateChunkData(int cx, int cz, std::vector<uint8_t> &out) const;

  /// @brief Chunk column that owns world coordinate `x`.
  static int ChunkCoord(int x) { return (x >= 0) ? x / kChunk : -((-x + kChunk - 1) / kChunk); }
  static int LocalCoord(int x) { return x - ChunkCoord(x) * kChunk; }

  /// @brief Ensures the chunk (cx, cz) is generated (thread-safe).
  void EnsureChunk(int cx, int cz);
  /// @brief True when the chunk has been generated already (thread-safe).
  [[nodiscard]] bool HasChunk(int cx, int cz) const;

  /// @brief Block at world coords (Air for y outside [0, kHeight) or a not-yet
  /// generated chunk). Thread-safe; code that touches a whole chunk should use
  /// FillSnapshot instead of many Get() calls.
  [[nodiscard]] Block Get(int x, int y, int z) const;

  /// @brief Sets a block (ignored when y is out of vertical range).
  void Set(int x, int y, int z, Block block);

  /// @brief Highest solid y in column (x, z); -1 for an empty column. The
  /// owning chunk is generated on demand. Ignores water / leaves / wood so it
  /// returns actual terrain for spawning.
  int SurfaceY(int x, int z);

  /// @brief Drops a generated chunk's block data (frees memory once the player
  /// walks away; a later visit regenerates it, losing edits made there).
  /// Ignored when the chunk is not generated.
  void UnloadChunk(int cx, int cz);

  /// @brief Copies chunk (cx, cz) plus its four XZ neighbours into `out` under
  /// a single lock (so the mesher needs no lock and no hash lookup per block).
  /// Returns false — leaving `out` unspecified — when any of the five chunk
  /// columns has not been generated yet.
  [[nodiscard]] bool FillSnapshot(int cx, int cz, ChunkSnapshot &out) const;

  /// @brief Number of generated chunk columns (diagnostics).
  [[nodiscard]] size_t ChunkCount() const;

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

  /// @brief Chunk data for the key, or nullptr when not generated. `mutex_` held.
  [[nodiscard]] const std::vector<uint8_t> *FindLocked(int cx, int cz) const;
  /// @brief Reads from a chunk that must already be generated.
  [[nodiscard]] Block GetLocalLocked(int cx, int cz, int lx, int ly, int lz) const;
  /// @brief Writes into chunk `cx,cz`'s local volume (assumes present).
  void         PutLocalLocked(int cx, int cz, int lx, int ly, int lz, Block block);
  unsigned int seed_;

  mutable std::mutex                                 mutex_;
  std::unordered_map<ChunkKey, std::vector<uint8_t>> chunks_;
};

/**
 * @brief A pad-1 copy of one chunk column: the chunk's own blocks plus a
 * one-block border sampled from the four XZ neighbour chunks.
 *
 * It is filled by `World::FillSnapshot` (a single lock acquisition) and is read
 * afterwards without any synchronization. That is what lets worker threads mesh
 * a chunk while the world keeps streaming: no lock taken during meshing, no
 * hash lookup per block, and a stable view even when a neighbour chunk is
 * generated concurrently.
 */
class ChunkSnapshot {
 public:
  static constexpr int kPad  = World::kChunk + 2;  // padded extent on X and Z
  static constexpr int kSize = kPad * kPad * World::kHeight;

  /// @brief Element index inside the padded volume; `px`/`pz` are pad-space
  /// (chunk-local + 1), `py` is world-space y.
  static constexpr size_t Index(int px, int py, int pz) {
    return (static_cast<size_t>(py) * kPad + static_cast<size_t>(pz)) * kPad + static_cast<size_t>(px);
  }

  /// @brief Block at chunk-local (lx, ly, lz); lx/lz may range over
  /// [-1, kChunk] (the neighbour border). Anything else yields Air.
  [[nodiscard]] Block At(int lx, int ly, int lz) const {
    if (ly < 0 || ly >= World::kHeight || lx < -1 || lx > World::kChunk || lz < -1 || lz > World::kChunk) {
      return Block::Air;
    }
    return static_cast<Block>(blocks_[Index(lx + 1, ly, lz + 1)]);
  }

 private:
  friend class World;

  std::vector<uint8_t> blocks_ = std::vector<uint8_t>(kSize, 0u);
};

/// @brief Builds the visible mesh for the chunk at (chunk_x, chunk_z) into
/// world-space vertices. `snapshot` must have been filled by
/// `World::FillSnapshot` for the same chunk (chunk + its XZ neighbours), so no
/// `World` access happens here — this is the function the worker threads call.
///
/// Opaque blocks land in `out_vertices`/`out_indices`; translucent water
/// surfaces land in `water_vertices`/`water_indices` so the caller can give
/// them a separate alpha-blended material.
void BuildChunkMesh(const ChunkSnapshot &snapshot, const Atlas &atlas, int chunk_x, int chunk_z,
                    std::vector<MEngine::Vertex> &out_vertices, std::vector<uint32_t> &out_indices,
                    std::vector<MEngine::Vertex> &water_vertices, std::vector<uint32_t> &water_indices);

}  // namespace vox
