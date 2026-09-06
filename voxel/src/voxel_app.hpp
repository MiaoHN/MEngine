/**
 * @file voxel_app.hpp
 * @brief The Minecraft-like voxel example application.
 *
 * A standalone MEngine application (built as its own target) that consumes the
 * engine's public API: Scene entities + MeshComponent for chunk meshes, the pbr
 * material pipeline for lighting/shadows/skybox, and GLFW through Application.
 * All voxel logic (world / meshing / player physics / picking) lives in this
 * example, not in the engine.
 */

#pragma once

#include <array>
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
  std::unique_ptr<World>          world_;
  Atlas                           atlas_;

  MEngine::Ref<MEngine::Material> chunk_material_;
  MEngine::Ref<MEngine::Material> ghost_material_;

  std::vector<MEngine::Entity> chunk_entities_;  // size ChunkCount()^2
  MEngine::Entity              ghost_entity_;
  bool                         ghost_ready_ = false;

  /// @brief Rebuilds the mesh of chunk (cx, cz) (and keeps its entity).
  void RemeshChunk(int cx, int cz);

  // --- player ---------------------------------------------------------------
  glm::vec3 position_{0.0f};  // feet-centre position
  glm::vec3 velocity_{0.0f};
  float     yaw_   = 0.0f;   // degrees
  float     pitch_ = -18.0f;  // degrees
  bool      flying_ = false;
  bool      captured_ = true;

  static constexpr float kHalfWidth = 0.30f;
  static constexpr float kHeight    = 1.80f;
  static constexpr float kEye       = 1.62f;
  static constexpr float kWalkSpeed = 4.5f;
  static constexpr float kFlySpeed  = 12.0f;
  static constexpr float kGravity   = 26.0f;
  static constexpr float kJump      = 9.2f;

  [[nodiscard]] bool BoxHitsSolid(const glm::vec3 &pos) const;
  [[nodiscard]] bool IsGrounded() const;

  // --- picking --------------------------------------------------------------
  struct Pick {
    bool       hit = false;
    glm::ivec3 block{0};  // the solid block hit
    glm::ivec3 place{0};  // adjacent air cell to place into
  };
  [[nodiscard]] Pick PickBlock() const;

  void RemeshAround(int x, int z);

  // --- hotbar ---------------------------------------------------------------
  std::vector<Block> hotbar_{Block::Grass, Block::Dirt, Block::Stone, Block::Sand, Block::Wood, Block::Leaves};
  int hotbar_index_ = 0;

  // edge detection for click actions
  bool prev_lmb_ = false;
  bool prev_rmb_ = false;
};

}  // namespace vox
