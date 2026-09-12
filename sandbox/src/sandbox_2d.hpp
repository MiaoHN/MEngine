/**
 * @file sandbox_2d.hpp
 * @brief 2D sandbox: sprites, a texture-sheet animation and an orthographic
 * camera, all on the engine's public API (see sandbox_3d.hpp for the 3D one).
 *
 * Also acts as the standalone 2D scene player: `--scene <path>` loads a scene
 * saved by the editor (the editor's Launch button picks this executable whenever
 * the scene's primary camera is orthographic).
 */

#pragma once

#include <memory>

#include "core/application.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/sprite.hpp"
#include "render/texture.hpp"
#include "scene/camera.hpp"
#include "scene/component.hpp"
#include "scene/entity.hpp"
#include "scene/scene.hpp"

namespace MEngine {

class Sandbox2D : public MEngine::Application {
 public:
  Sandbox2D();
  ~Sandbox2D();

  void Initialize() override;
  void OnUpdate(float dt) override;

 private:
  /// @brief Builds the demo scene (procedural textures + sprites).
  void BuildDemoScene();

  /// @brief Moves the player sprite, drives its animation and follows it with
  /// the camera.
  void UpdatePlayer(float dt);

  std::shared_ptr<Scene> active_scene_;

  /// True when a scene file was loaded (`--scene`): the demo is not built and
  /// the scene is simulated/rendered as saved instead.
  bool running_loaded_scene_ = false;

  // --- demo content ---------------------------------------------------------
  Entity player_;
  Entity camera_entity_;

  Ref<Texture> ground_texture_;
  Ref<Texture> character_texture_;  // 4x2 sheet: walk cycle + gem animation
  SpriteSheet  character_sheet_{4, 2};

  glm::vec2 player_velocity_{0.0f};
  bool      player_flip_x_ = false;

  static constexpr float kPlayerSpeed   = 4.0f;
  static constexpr float kPlayerSize    = 64.0f;  // texture pixels
  static constexpr float kPixelsPerUnit = 32.0f;  // 32 px == 1 world unit
};

}  // namespace MEngine
