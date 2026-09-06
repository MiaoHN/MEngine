/**
 * @file voxel_app.hpp
 * @brief The Minecraft-like voxel example application.
 *
 * A standalone MEngine application (built as its own target) that consumes the
 * engine's public API: Scene entities + MeshComponent for chunk meshes, the pbr
 * material pipeline for lighting/shadows/skybox, and GLFW through Application.
 * All voxel logic (world / meshing / player physics / picking) lives here.
 */

#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "core/application.hpp"
#include "scene/entity.hpp"
#include "scene/scene.hpp"

#include "voxel_atlas.hpp"
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

  // Streaming: chunks around the player, each a Scene entity.
  static constexpr int kRadius = 5;  // visible chunk radius
  struct ChunkTile {
    int             cx = 0, cz = 0;
    MEngine::Entity entity;
  };
  std::vector<ChunkTile> tiles_;
  int active_cx_ = 0x7fffffff;
  int active_cz_ = 0x7fffffff;

  void RebuildChunksAround(int center_cx, int center_cz);
  void RemeshChunk(int cx, int cz);  // no-op when not loaded

  // --- player ---------------------------------------------------------------
  glm::vec3 position_{0.0f};  // feet-centre position
  glm::vec3 velocity_{0.0f};
  float     yaw_   = 0.0f;   // degrees
  float     pitch_ = -18.0f; // degrees
  bool      flying_ = false;
  bool      captured_ = true;
  float     last_space_ = -10.0f;
  int       spawn_x_ = 0, spawn_z_ = 0;  // dry-land spawn point (found at start)

  static constexpr float kHalfWidth = 0.30f;
  static constexpr float kHeight    = 1.80f;
  static constexpr float kEye       = 1.62f;
  static constexpr float kWalkSpeed = 4.5f;
  static constexpr float kFlySpeed  = 14.0f;
  static constexpr float kGravity   = 26.0f;
  static constexpr float kJump      = 9.2f;

  [[nodiscard]] bool BoxHitsSolid(const glm::vec3 &pos) const;
  [[nodiscard]] bool IsGrounded() const;
  void Respawn();

  // --- picking --------------------------------------------------------------
  struct Pick {
    bool       hit = false;
    glm::ivec3 block{0};
    glm::ivec3 place{0};
  };
  [[nodiscard]] Pick PickBlock() const;

  std::vector<Block> hotbar_{Block::Grass, Block::Dirt, Block::Stone, Block::Sand, Block::Wood, Block::Leaves};
  int hotbar_index_ = 0;

  bool prev_lmb_ = false;
  bool prev_rmb_ = false;
};

}  // namespace vox
