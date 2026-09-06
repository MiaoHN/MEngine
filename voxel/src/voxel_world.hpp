/**
 * @file voxel_world.hpp
 * @brief Dense voxel world storage + procedural terrain for the voxel demo.
 *
 * All game-side (block storage, terrain generation, chunk meshing) lives here
 * in namespace `vox`; the engine is only used through its public Mesh / Vertex
 * types and, in the app, Scene rendering.
 */

#pragma once

#include <cstdint>
#include <vector>

#include "render/vertex.hpp"

#include "voxel_atlas.hpp"

namespace vox {

/// @brief A dense, fixed-size voxel region (XZ = chunk_count * 16, height H).
/// Out-of-range blocks read as Air, writes are ignored — the world has edges.
class World {
 public:
  static constexpr int kChunk = 16;

  World(int chunk_count = 9, int height = 40, unsigned int seed = 1337u);

  void SetSeed(unsigned int seed) { seed_ = seed; }
  [[nodiscard]] int ChunkCount() const { return chunk_count_; }
  [[nodiscard]] int SizeXZ() const { return chunk_count_ * kChunk; }
  [[nodiscard]] int Height() const { return height_; }

  /// @brief Block at world coords (Air outside the region).
  [[nodiscard]] Block Get(int x, int y, int z) const;
  /// @brief Writes a block (ignored outside the region).
  void Set(int x, int y, int z, Block block);

  /// @brief Highest solid y at column (x, z), or -1 for an air column.
  [[nodiscard]] int SurfaceY(int x, int z) const;

  /// @brief True when a solid block occupies the given integer cell.
  [[nodiscard]] bool IsSolidCell(int x, int y, int z) const { return IsSolid(Get(x, y, z)); }

 private:
  void Generate();
  void PlantTree(int x, int y, int z);

  int           chunk_count_;
  int           height_;
  unsigned int  seed_;
  std::vector<uint8_t> blocks_;  // Index(x,y,z) in world coords
};

/// @brief Builds the visible mesh (verts+indices, engine Vertex layout) for the
/// chunk at (chunk_x, chunk_z). Faces against air/out-of-bounds are emitted;
/// UVs pick the correct atlas tile per block face. World-coordinate vertices.
void BuildChunkMesh(const World &world, const Atlas &atlas, int chunk_x, int chunk_z,
                    std::vector<MEngine::Vertex> &out_vertices, std::vector<uint32_t> &out_indices);

}  // namespace vox
