/**
 * @file sandbox_2d.cpp
 * @brief 2D sandbox / standalone 2D scene player.
 *
 * Everything here is a normal Scene entity so that 2D and 3D content use the
 * same machinery:
 *
 *  - an orthographic primary camera (that is what makes the editor treat the
 *    scene as 2D),
 *  - a tiled background and a spawn of animated sprites (`SpriteComponent` +
 *    `SpriteAnimationComponent`), sorted by (sorting layer, order in layer),
 *  - a player sprite moved with WASD whose sheet animation only plays while it
 *    moves, with the camera following it.
 *
 * The two textures are generated procedurally (no binary assets needed), the
 * same trick the voxel demo uses for its block atlas.
 */

#include "sandbox_2d.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/input.hpp"
#include "core/logger.hpp"
#include "render/asset_manager.hpp"
#include "scene/camera.hpp"
#include "scene/component.hpp"
#include "utils/profiler.h"

using namespace MEngine;

namespace {

constexpr int kCell = 32;  // texture cell size of the generated sheet

/// @brief Writes one RGBA pixel if it is inside the texture.
///
/// `y` counts DOWN from the top of the image (the drawing helpers below are
/// written screen-style) and is flipped on write: `Texture::SetData` uploads
/// buffer row 0 as the BOTTOM of the texture, so this keeps the generators'
/// "row 0 = the top row of the sheet" consistent with `SpriteSheet::FrameRect`,
/// which addresses frame 0 at the top-left of the sheet.
void PutPixel(std::vector<unsigned char> &rgba, int width, int height, int x, int y, const glm::vec4 &color) {
  if (x < 0 || y < 0 || x >= width || y >= height) {
    return;
  }
  const int    row    = height - 1 - y;
  const size_t offset = (static_cast<size_t>(row) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
  const auto   to_byte = [](float v) { return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
  rgba[offset + 0] = to_byte(color.r);
  rgba[offset + 1] = to_byte(color.g);
  rgba[offset + 2] = to_byte(color.b);
  rgba[offset + 3] = to_byte(color.a);
}

/// @brief Fills an axis-aligned rectangle (in texture pixels) with one colour.
void FillRect(std::vector<unsigned char> &rgba, int width, int height, int x0, int y0, int x1, int y1,
              const glm::vec4 &color) {
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      PutPixel(rgba, width, height, x, y, color);
    }
  }
}

/// @brief Fills a circle (texture pixels) with one colour.
void FillCircle(std::vector<unsigned char> &rgba, int width, int height, int cx, int cy, int radius,
                const glm::vec4 &color) {
  for (int y = cy - radius; y <= cy + radius; ++y) {
    for (int x = cx - radius; x <= cx + radius; ++x) {
      const int dx = x - cx;
      const int dy = y - cy;
      if (dx * dx + dy * dy <= radius * radius) {
        PutPixel(rgba, width, height, x, y, color);
      }
    }
  }
}

/// @brief A 2x2 checker tile used for the ground (tiles seamlessly).
Ref<Texture> MakeGroundTexture() {
  constexpr int kSize = 32;
  std::vector<unsigned char> rgba(static_cast<size_t>(kSize) * kSize * 4u, 0u);
  const glm::vec4 dark{0.20f, 0.26f, 0.33f, 1.0f};
  const glm::vec4 light{0.25f, 0.32f, 0.40f, 1.0f};
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      const bool checker = ((x / (kSize / 2)) + (y / (kSize / 2))) % 2 == 0;
      PutPixel(rgba, kSize, kSize, x, y, checker ? dark : light);
    }
  }

  auto texture = CreateRef<Texture>();
  texture->SetData(rgba.data(), kSize, kSize);
  return texture;
}

/// @brief Generates a 4x2 sheet (128x64): row 0 = a 4-frame walk cycle, row 1 =
/// a 4-frame spinning gem. Both animate an "arm/leg" or "facet" shape so the
/// animation is obvious in a screenshot.
Ref<Texture> MakeCharacterSheet() {
  constexpr int kCols = 4;
  constexpr int kRows = 2;
  constexpr int kWidth  = kCols * kCell;
  constexpr int kHeight = kRows * kCell;
  std::vector<unsigned char> rgba(static_cast<size_t>(kWidth) * kHeight * 4u, 0u);

  const glm::vec4 skin{0.96f, 0.80f, 0.62f, 1.0f};
  const glm::vec4 body{0.36f, 0.62f, 0.94f, 1.0f};
  const glm::vec4 body_dark{0.24f, 0.44f, 0.72f, 1.0f};
  const glm::vec4 boot{0.30f, 0.22f, 0.18f, 1.0f};
  const glm::vec4 gem_a{0.95f, 0.36f, 0.42f, 1.0f};
  const glm::vec4 gem_b{0.99f, 0.78f, 0.30f, 1.0f};
  const glm::vec4 gem_c{0.42f, 0.92f, 0.72f, 1.0f};
  const glm::vec4 gem_d{0.62f, 0.52f, 0.95f, 1.0f};

  // --- row 0: walk cycle -----------------------------------------------------
  for (int frame = 0; frame < kCols; ++frame) {
    const int ox = frame * kCell;
    const int oy = 0;  // top row (v is flipped later by the sheet math)
    const int step = (frame % 2 == 0) ? 1 : -1;  // alternate the stride

    // head + torso (static)
    FillCircle(rgba, kWidth, kHeight, ox + kCell / 2, oy + 9, 6, skin);
    FillRect(rgba, kWidth, kHeight, ox + 10, oy + 15, ox + 21, oy + 24, body);
    // arms swing with the stride
    FillRect(rgba, kWidth, kHeight, ox + 7, oy + 16 + step, ox + 9, oy + 21 + step, body_dark);
    FillRect(rgba, kWidth, kHeight, ox + 22, oy + 16 - step, ox + 24, oy + 21 - step, body_dark);
    // legs alternate
    FillRect(rgba, kWidth, kHeight, ox + 11, oy + 25, ox + 14, oy + 27 + step, body_dark);
    FillRect(rgba, kWidth, kHeight, ox + 17, oy + 25, ox + 20, oy + 27 - step, body_dark);
    FillRect(rgba, kWidth, kHeight, ox + 10, oy + 28 + step, ox + 15, oy + 29 + step, boot);
    FillRect(rgba, kWidth, kHeight, ox + 16, oy + 28 - step, ox + 21, oy + 29 - step, boot);
  }

  // --- row 1: spinning gem ---------------------------------------------------
  const glm::vec4 facets[4] = {gem_a, gem_b, gem_c, gem_d};
  for (int frame = 0; frame < kCols; ++frame) {
    const int ox = frame * kCell;
    const int oy = kCell;  // bottom row
    const int cx = ox + kCell / 2;
    const int cy = oy + kCell / 2;
    // Diamond outline whose width pulses with the frame (a cheap "spin").
    const int half_w = 4 + frame * 2;
    const int half_h = 10 - frame;
    for (int dy = -half_h; dy <= half_h; ++dy) {
      const int span = half_w - std::abs(dy) * (half_w - 2) / std::max(1, half_h);
      FillRect(rgba, kWidth, kHeight, cx - span, cy + dy, cx + span, cy + dy, facets[frame]);
    }
  }

  auto texture = CreateRef<Texture>();
  texture->SetData(rgba.data(), kWidth, kHeight);
  return texture;
}

}  // namespace

Sandbox2D::Sandbox2D() : Application(Application::GetStartupApi()) {
  active_scene_ = std::make_shared<Scene>();

  const std::string &scene_path = Application::GetStartupScenePath();
  if (!scene_path.empty()) {
    active_scene_->LoadScene(scene_path);
    active_scene_->StartSimulation();
    active_scene_->GetScriptEngine().StartAll();
    running_loaded_scene_ = true;
    LOG_INFO("Sandbox2D") << "Loaded 2D scene '" << scene_path << "'";
    return;
  }

  BuildDemoScene();
}

Sandbox2D::~Sandbox2D() {}

void Sandbox2D::BuildDemoScene() {
  // --- scene defaults for 2D ------------------------------------------------
  // Declaring the scene 2D selects the sprite-only render path (no lights,
  // shadows, SSAO, skybox, HDR buffer or post-processing) and applies the 2D
  // render defaults; the camera below only defines the view.
  active_scene_->SetDimension(SceneDimension::Scene2D);
  active_scene_->SetBackgroundColor(glm::vec3(0.07f, 0.09f, 0.14f));
  active_scene_->SetExposure(1.0f);

  // --- camera ---------------------------------------------------------------
  camera_entity_ = active_scene_->CreateEntity("Main Camera");
  auto &camera    = camera_entity_.AddComponent<CameraComponent>();
  camera.primary  = true;
  camera.camera.projection_type = ProjectionType::Orthographic;
  camera.camera.ortho_size      = 6.0f;  // half-height: 12 world units visible
  camera.camera.near_plane      = 0.1f;
  camera.camera.far_plane       = 100.0f;
  camera.camera.position        = glm::vec3(0.0f, 0.0f, 10.0f);
  camera.camera.rotation        = glm::vec3(0.0f, 0.0f, 0.0f);  // looking down -Z

  // --- textures -------------------------------------------------------------
  ground_texture_    = MakeGroundTexture();
  character_texture_ = MakeCharacterSheet();

  // --- tiled ground (many sprites, ONE draw: same mesh + material content) ---
  constexpr int kTilesX = 40;
  constexpr int kTilesY = 20;
  for (int ty = 0; ty < kTilesY; ++ty) {
    for (int tx = 0; tx < kTilesX; ++tx) {
      Entity tile = active_scene_->CreateEntity("Ground");
      auto  &tr   = tile.AddComponent<Transform>();
      tr.translation = glm::vec3(static_cast<float>(tx) - kTilesX * 0.5f, static_cast<float>(ty) - kTilesY * 0.5f,
                                 0.0f);
      auto &sprite = tile.AddComponent<SpriteComponent>(ground_texture_);
      sprite.size          = glm::vec2(1.0f, 1.0f);
      sprite.color         = glm::vec4(1.0f, 1.0f, 1.0f, 0.35f);
      sprite.sorting_layer = -100;
    }
  }

  // --- player (animated walk cycle) -----------------------------------------
  player_ = active_scene_->CreateEntity("Player");
  {
    auto &tr       = player_.AddComponent<Transform>();
    tr.translation = glm::vec3(0.0f, 0.0f, 0.0f);

    auto &sprite = player_.AddComponent<SpriteComponent>(character_texture_);
    sprite.size          = glm::vec2(kPlayerSize / kPixelsPerUnit, kPlayerSize / kPixelsPerUnit);
    sprite.sorting_layer = 10;
    sprite.SetSheetFrame(character_sheet_, 0);

    auto &animation          = player_.AddComponent<SpriteAnimationComponent>();
    animation.sheet          = character_sheet_;
    animation.frame_count    = character_sheet_.columns;  // row 0 = walk cycle
    animation.fps            = 8.0f;
    animation.loop           = true;
    animation.playing        = false;  // only animates while walking
  }

  // --- animated gems (same sheet, second row) -------------------------------
  for (int i = 0; i < 7; ++i) {
    Entity gem = active_scene_->CreateEntity("Gem");
    auto  &tr  = gem.AddComponent<Transform>();
    const float angle = static_cast<float>(i) / 7.0f * 6.2831853f;
    tr.translation    = glm::vec3(std::cos(angle) * 6.0f, std::sin(angle) * 3.5f, 0.0f);

    auto &sprite = gem.AddComponent<SpriteComponent>(character_texture_);
    sprite.size          = glm::vec2(1.0f, 1.0f);
    sprite.sorting_layer = 5;
    sprite.SetSheetFrame(character_sheet_, character_sheet_.columns + i % 4);

    auto &animation       = gem.AddComponent<SpriteAnimationComponent>();
    animation.sheet       = character_sheet_;
    animation.first_frame = character_sheet_.columns;  // row 1
    animation.frame_count = character_sheet_.columns;
    animation.fps         = 6.0f + static_cast<float>(i);  // desynchronized
    animation.loop        = true;
  }

  // Sun is irrelevant for unlit sprites but keeps the scene graph sane for
  // tools that expect a light (no shadows/IBL in this scene).
  LOG_INFO("Sandbox2D") << "Demo scene: " << kTilesX * kTilesY << " ground sprites + player + 7 animated gems "
                        << "(orthographic camera, no skybox)";

  // Start the (physics-free) simulation so the per-frame systems run: the
  // sprite sheet animations advance in StepSimulation, exactly like they do for
  // an editor scene played back through this sandbox.
  active_scene_->StartSimulation();
}

void Sandbox2D::Initialize() {
  LOG_INFO("Sandbox2D") << "Controls: WASD/arrows move the player (the camera follows), Esc quits";
}

void Sandbox2D::UpdatePlayer(float dt) {
  if (player_.GetHandle() == entt::null || !player_.HasComponent<Transform>()) {
    return;
  }

  float dx = 0.0f;
  float dy = 0.0f;
  if (Input::IsKeyPressed(GLFW_KEY_A) || Input::IsKeyPressed(GLFW_KEY_LEFT)) dx -= 1.0f;
  if (Input::IsKeyPressed(GLFW_KEY_D) || Input::IsKeyPressed(GLFW_KEY_RIGHT)) dx += 1.0f;
  if (Input::IsKeyPressed(GLFW_KEY_S) || Input::IsKeyPressed(GLFW_KEY_DOWN)) dy -= 1.0f;
  if (Input::IsKeyPressed(GLFW_KEY_W) || Input::IsKeyPressed(GLFW_KEY_UP)) dy += 1.0f;

  const glm::vec2 wish(dx, dy);
  const float     length = glm::length(wish);
  const glm::vec2 dir    = (length > 1e-4f) ? wish / length : glm::vec2(0.0f);
  player_velocity_       = dir * kPlayerSpeed;

  auto &transform = player_.GetComponent<Transform>();
  transform.translation += glm::vec3(player_velocity_ * dt, 0.0f);

  if (player_.HasComponent<SpriteComponent>() && player_.HasComponent<SpriteAnimationComponent>()) {
    auto &sprite    = player_.GetComponent<SpriteComponent>();
    auto &animation = player_.GetComponent<SpriteAnimationComponent>();
    const bool moving = length > 1e-4f;
    animation.playing = moving;  // idle at frame 0 while standing still
    if (moving && std::abs(dx) > 1e-4f) {
      player_flip_x_ = dx < 0.0f;
    }
    sprite.flip_x = player_flip_x_;
  }

  // Follow the player with the orthographic camera (a slight vertical lead so
  // the view shows where the player is going).
  if (camera_entity_.GetHandle() != entt::null && camera_entity_.HasComponent<CameraComponent>()) {
    auto &camera        = camera_entity_.GetComponent<CameraComponent>().camera;
    const glm::vec3 target(transform.translation.x, transform.translation.y + 0.5f, camera.position.z);
    camera.position = glm::mix(camera.position, target, std::min(1.0f, 6.0f * dt));
  }
}

void Sandbox2D::OnUpdate(float dt) {
  PROFILER_FUNCTION();

  if (running_loaded_scene_) {
    active_scene_->StepSimulation(dt);
    active_scene_->GetScriptEngine().Update(dt);
    active_scene_->UpdateCameraControllers(dt, Input::GetMouseDelta(),
                                           Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT));
    if (active_scene_->HasPrimaryCamera()) {
      active_scene_->RenderFromPrimaryCamera();
    }
    return;
  }

  UpdatePlayer(dt);
  active_scene_->StepSimulation(dt);  // advances the sprite sheet animations
  active_scene_->RenderFromPrimaryCamera();
}

Application *CreateApplication() { return new Sandbox2D(); }
