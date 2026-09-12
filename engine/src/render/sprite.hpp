/**
 * @file sprite.hpp
 * @brief 2D sprite helpers: texture-sheet frame math, the shared unit quad
 * (cached per UV rectangle) and the unlit/alpha-blended material sprites draw
 * with.
 *
 * A sprite is a plain Scene entity (`Transform` + `SpriteComponent`) rendered
 * by `Scene::RenderMeshes`, exactly like a mesh or model entity. That is what
 * lets 2D and 3D content live in the SAME scene: one pass draws everything, and
 * sprites (which are translucent so they never write depth) are ordered by
 * (sorting layer, order in layer) inside the translucent pass.
 */

#pragma once

#include <algorithm>

#include <glm/glm.hpp>

#include "core/common.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/texture.hpp"

namespace MEngine {

/// @brief Uniform grid of frames inside one texture (a sprite sheet).
///
/// Frame 0 is the TOP-LEFT cell; frames run left→right, then top→bottom.
/// `FrameRect` returns the normalized UV rectangle (u0, v0, u1, v1) for a frame,
/// with v pointing up because textures are uploaded bottom-up by the loaders.
struct SpriteSheet {
  int columns = 1;
  int rows    = 1;

  SpriteSheet() = default;
  SpriteSheet(int columns, int rows) : columns(columns), rows(rows) {}

  /// @brief Number of frames in the sheet (at least 1).
  [[nodiscard]] int FrameCount() const { return std::max(1, columns) * std::max(1, rows); }

  /// @brief Normalized (u0, v0, u1, v1) rectangle of `frame` (wrapped into range).
  [[nodiscard]] glm::vec4 FrameRect(int frame) const;
};

/// @brief Shared 1x1 sprite quad in the XY plane (centered, facing +Z, uv (0,0)
/// at the bottom-left) whose texture coordinates cover `uv_rect`.
///
/// Quads are cached per (rect, flip) so every sprite showing the same sheet
/// frame shares one `Mesh` — and therefore batches into a single instanced draw
/// when the material matches too. Render thread only.
[[nodiscard]] Ref<Mesh> GetSpriteQuad(const glm::vec4 &uv_rect, bool flip_x = false, bool flip_y = false);

/// @brief Unlit, alpha-blended, double-sided material for sprites: albedo =
/// `texture` (its alpha is used), tinted by `color` (rgb + opacity).
///
/// Sprites own their material (cheap: it is a handful of Refs), and materials
/// with identical content are merged by the renderer's batching, so many
/// sprites sharing a texture and tint still go out in one draw.
[[nodiscard]] Ref<Material> CreateSpriteMaterial(const Ref<Texture> &texture, const glm::vec4 &color);

}  // namespace MEngine
