/**
 * @file voxel_app.hpp
 * @brief The Minecraft-like voxel example application.
 *
 * A standalone MEngine application (built as its own target) that consumes the
 * engine's public API: Scene entities + MeshComponent for chunk meshes, the pbr
 * material pipeline for lighting/shadows/skybox, and GLFW through Application.
 * All voxel logic (world / meshing / player physics / picking) lives here.
 *
 * Chunk streaming is asynchronous (`voxel_streamer.hpp`): a worker pool
 * generates and meshes chunks off the render thread, and this class only
 * uploads a budget-limited number of finished meshes per frame, so moving
 * through the world never stalls rendering.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "core/application.hpp"
#include "scene/entity.hpp"
#include "scene/scene.hpp"

#include "voxel_atlas.hpp"
#include "voxel_streamer.hpp"
#include "voxel_world.hpp"

namespace vox {

class VoxelApp : public MEngine::Application {
 public:
  explicit VoxelApp(MEngine::GraphicsAPI api);
  ~VoxelApp() override;

  void Initialize() override;
  void OnUpdate(float dt) override;

 private:
  // --- world / scene --------------------------------------------------------
  std::shared_ptr<MEngine::Scene> scene_;
  World                           world_;
  Atlas                           atlas_;

  MEngine::Ref<MEngine::Material> chunk_material_;
  MEngine::Ref<MEngine::Material> water_material_;  // translucent, double-sided

  // Streaming: chunks around the player, each a Scene entity. A chunk with
  // water gets a second translucent entity (tile.water) drawn blended over the
  // opaque terrain. The GPU side is driven from `upload_queue_` (finished CPU
  // meshes handed over by the background streamer), never by the frame loop
  // building chunks synchronously.
  //
  // Two radii, Minecraft style: `kRadius` is the area that actively streams
  // (generated + meshed as the player moves), `keep_radius_` is the larger area
  // of ALREADY loaded chunks that stay resident — data + entities + meshes are
  // kept and still rendered, they just stop being updated, so terrain you
  // walked through does not vanish when you look back.
  static constexpr int kRadius        = 5;  // active load radius (chunks)
  static constexpr int kMinKeepRadius = kRadius + 2;
  static constexpr int kMaxKeepRadius = 24;
  int                  keep_radius_   = kRadius + 3;  // resident radius (MENGINE_VOXEL_KEEP)

  struct ChunkTile {
    MEngine::Entity entity;  // opaque terrain mesh (may be null)
    MEngine::Entity water;   // translucent water surface (may be null)
  };
  std::unordered_map<int64_t, ChunkTile> tiles_;

  /// Chunks that left the keep ring, waiting to be parked (see `free_tiles_`).
  /// Parking strips the MeshComponents — a few per frame, since releasing a few
  /// dozen chunk meshes at once stalls the driver — and keeps the entities.
  std::deque<std::pair<int64_t, ChunkTile>> retired_;

  /// Parked entities (no MeshComponent, therefore invisible) reused when a new
  /// chunk needs a tile. Destroying/recreating Scene entities goes through the
  /// engine's bookkeeping (and, in debug builds, the logger), which costs far
  /// more than re-adding a MeshComponent — chunk streaming touches dozens of
  /// tiles per second, so the pool keeps the frame cost flat.
  std::vector<ChunkTile> free_tiles_;

  std::unique_ptr<ChunkStreamer> streamer_;
  std::vector<ChunkMesh>         upload_queue_;  // finished CPU meshes awaiting GPU upload

  int active_cx_ = 0x7fffffff;
  int active_cz_ = 0x7fffffff;

  /// @brief Re-centres the wanted chunk set and drops the tiles that left it.
  void UpdateStreaming(int center_cx, int center_cz);
  /// @brief Uploads finished meshes until `deadline` / `max_count`, always at
  /// least one so streaming keeps making progress.
  void UploadFinishedChunks(std::chrono::steady_clock::time_point deadline, int max_count);
  /// @brief Creates/updates the entities of one finished chunk mesh.
  void ApplyChunkMesh(ChunkMesh &&mesh);
  /// @brief Moves a tile out of the visible set into the retired queue (it is
  /// parked — meshes released, entities pooled — a few per frame).
  void RetireTile(int64_t key);
  /// @brief Takes a retired tile back (returns false when it is not pooled).
  [[nodiscard]] bool TakeRetiredTile(int64_t key, ChunkTile &out);
  /// @brief Pops a parked tile from the entity pool (empty tile when none is).
  [[nodiscard]] ChunkTile TakeFreeTile();
  /// @brief Parks a bounded number of retired tiles (releases their meshes and
  /// pools their entities); count + shared frame deadline.
  void DrainRetiredTiles(std::chrono::steady_clock::time_point deadline, int max_count);
  /// @brief Releases a tile's GPU mesh (keeps the entity for reuse).
  void DetachMesh(MEngine::Entity entity);
  /// @brief Really destroys a tile's entities (pool overflow / shutdown path).
  void DestroyTileEntities(ChunkTile &tile);
  /// @brief True when the 3x3 chunks around (cx, cz) are meshed: gates the
  /// player physics so we never fall through a chunk that is still loading.
  [[nodiscard]] bool GroundReady(int cx, int cz) const;
  /// @brief Waits (bounded) for the chunks around the player to be meshed so
  /// the first frames already show ground instead of an empty sky.
  void PrewarmSpawn(int center_cx, int center_cz);
  /// @brief True when `entity` is still alive in the scene registry.
  [[nodiscard]] bool IsAlive(MEngine::Entity entity) const;

  // --- player ---------------------------------------------------------------
  glm::vec3 position_{0.0f};  // feet-centre position
  glm::vec3 velocity_{0.0f};
  float     yaw_        = 0.0f;    // degrees
  float     pitch_      = -18.0f;  // degrees
  bool      flying_     = false;
  bool      captured_   = true;
  float     last_space_ = -10.0f;
  /// Unattended-test hook (`MENGINE_VOXEL_AUTOWALK=<blocks/s>`, 0 = off):
  /// flies the player along +X so chunk streaming can be verified headlessly
  /// (the camera keeps its own direction, see MENGINE_VOXEL_DEBUG_CAM).
  float autowalk_speed_ = 0.0f;
  int   spawn_x_ = 0, spawn_z_ = 0;  // dry-land spawn point (found at start)

  static constexpr float kHalfWidth = 0.30f;
  static constexpr float kHeight    = 1.80f;
  static constexpr float kEye       = 1.62f;
  static constexpr float kWalkSpeed = 4.5f;
  static constexpr float kFlySpeed  = 14.0f;
  static constexpr float kGravity   = 26.0f;
  static constexpr float kJump      = 9.2f;

  [[nodiscard]] bool BoxHitsSolid(const glm::vec3 &pos) const;
  [[nodiscard]] bool IsGrounded() const;
  void               Respawn();

  // --- picking --------------------------------------------------------------
  struct Pick {
    bool       hit = false;
    glm::ivec3 block{0};
    glm::ivec3 place{0};
  };
  [[nodiscard]] Pick PickBlock() const;

  std::vector<Block> hotbar_{Block::Grass, Block::Dirt, Block::Stone, Block::Sand, Block::Wood, Block::Leaves};
  int                hotbar_index_ = 0;

  bool prev_lmb_ = false;
  bool prev_rmb_ = false;
};

}  // namespace vox
