/**
 * @file voxel_atlas.hpp
 * @brief Procedural block-texture atlas + block registry for the voxel demo.
 *
 * Everything here lives in the voxel example (namespace `vox`). It builds a
 * small RGBA atlas in CPU memory (16x16 tiles with a 1 px replicated border to
 * stop bleeding under linear filtering) and answers "which atlas tile does a
 * block face use" + "where is that tile in UV space". The engine's Texture /
 * pbr material are only used as consumers of this pixel buffer.
 */

#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace vox {

// --- Block registry ---------------------------------------------------------

enum class Block : uint8_t {
  Air = 0,
  Grass,
  Dirt,
  Stone,
  Sand,
  Wood,
  Leaves,
  Water,
  CoalOre,
  Gravel,
  Count,
};

/// @brief True when a block occludes its neighbours (used for face culling and
/// opaque rendering; includes water and leaves).
inline bool IsOpaque(Block b) { return b != Block::Air; }
/// @brief True when the player's collider should collide with the block
/// (water is passable).
inline bool IsCollidable(Block b) { return b != Block::Air && b != Block::Water; }
/// @brief True when a block hides its neighbours' faces during MESHING
/// (opaque solids only - water is translucent and must never hide a terrain
/// face, otherwise underwater coastlines render as hollow "clipping" holes).
inline bool IsOccluder(Block b) { return b != Block::Air && b != Block::Water; }

// --- Tile atlas -------------------------------------------------------------

enum TileId : uint8_t {
  TileGrassTop = 0,
  TileGrassSide,
  TileDirt,
  TileStone,
  TileSand,
  TileWoodSide,
  TileWoodTop,
  TileLeaves,
  TileWaterTop,
  TileCoal,
  TileGravel,
  TileCount,
};

/// @brief A CPU-only RGBA block-texture atlas. Owns its pixel buffer; give it
/// to an engine Texture via Texture::SetData to upload it as the albedo map.
class Atlas {
 public:
  static constexpr int kTile      = 16;  // content size of one tile (px)
  static constexpr int kStride    = 18;  // stride incl. 1 px replicated border
  static constexpr int kCols      = 6;   // tiles per row
  static constexpr int kRows      = 2;
  static constexpr int kWidth     = kCols * kStride;
  static constexpr int kHeight    = kRows * kStride;

  Atlas();

  [[nodiscard]] const std::vector<uint8_t> &Pixels() const { return rgba_; }
  [[nodiscard]] int Width() const { return kWidth; }
  [[nodiscard]] int Height() const { return kHeight; }

  /// @brief Atlas tile used by `block` for a face orientation (top / bottom /
  /// any side). Callers pick the face kind themselves.
  [[nodiscard]] TileId TileFor(Block block, bool top, bool bottom) const;

  /// @brief Converts a tile + fractional (u, v) in [0,1] to a UV coordinate
  /// inside the tile's CONTENT region (a margin keeps linear filter from
  /// bleeding into neighbouring tiles). `v` = 0 is the tile bottom.
  [[nodiscard]] glm::vec2 TileUV(TileId tile, float u, float v) const;

 private:
  /// @brief Fills one tile's 16x16 content. `row` counts from the bottom of the
  /// tile (0 = bottom, 15 = top) so GL row-0-is-bottom uploads the right way up.
  void DrawContent(TileId tile);

  std::vector<uint8_t> rgba_;
};

}  // namespace vox
