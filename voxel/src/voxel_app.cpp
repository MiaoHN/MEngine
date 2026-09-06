#include "voxel_app.hpp"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "core/input.hpp"
#include "core/logger.hpp"
#include "render/asset_manager.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/texture.hpp"

namespace vox {

using MEngine::Entity;
using MEngine::Mesh;
using MEngine::MeshComponent;
using MEngine::Ref;
using MEngine::Texture;
using MEngine::Transform;

namespace {

/// @brief Environment override for a new world seed (MENGINE_VOXEL_SEED).
/// Uses the MSVC-safe `_dupenv_s` on Windows (std::getenv is deprecated there).
unsigned int StartupSeed() {
#if defined(_WIN32)
  char *buffer = nullptr;
  size_t len   = 0;
  if (_dupenv_s(&buffer, &len, "MENGINE_VOXEL_SEED") == 0 && buffer != nullptr && buffer[0] != '\0') {
    const unsigned int seed = static_cast<unsigned int>(std::atoi(buffer));
    free(buffer);
    return seed;
  }
  if (buffer != nullptr) {
    free(buffer);
  }
#else
  const char *env = std::getenv("MENGINE_VOXEL_SEED");
  if (env != nullptr && env[0] != '\0') {
    return static_cast<unsigned int>(std::atoi(env));
  }
#endif
  return 1337u;
}

glm::vec3 FrontFrom(float yaw_deg, float pitch_deg) {
  const float yaw   = glm::radians(yaw_deg);
  const float pitch = glm::radians(pitch_deg);
  const float cp    = std::cos(pitch);
  return {cp * std::sin(yaw), std::sin(pitch), -cp * std::cos(yaw)};
}

glm::vec3 RightFrom(float yaw_deg) {
  return {std::cos(glm::radians(yaw_deg)), 0.0f, std::sin(glm::radians(yaw_deg))};
}

int FloorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }

}  // namespace

VoxelApp::VoxelApp(MEngine::GraphicsAPI api) : MEngine::Application(api), world_(StartupSeed()) {}

VoxelApp::~VoxelApp() {}

void VoxelApp::Initialize() {
  scene_ = std::make_shared<MEngine::Scene>();

  // --- shared chunk material (one pbr material + the procedural atlas) ------
  auto atlas_texture = MEngine::CreateRef<Texture>();
  atlas_texture->SetData(const_cast<unsigned char *>(atlas_.Pixels().data()), atlas_.Width(), atlas_.Height());

  auto shader = MEngine::AssetManager::Instance().GetShader("pbr");
  chunk_material_ = MEngine::CreateRef<MEngine::Material>();
  chunk_material_->SetShader(shader);
  chunk_material_->SetAlbedoMap(atlas_texture);
  chunk_material_->SetBaseColorFactor(glm::vec4(1.0f));
  chunk_material_->SetMetallicFactor(0.0f);
  chunk_material_->SetRoughnessFactor(0.9f);

  // --- scene lighting / post ------------------------------------------------
  scene_->GetLight().direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
  scene_->GetLight().color     = glm::vec3(1.35f);
  scene_->SetIblIntensity(0.28f);
  scene_->SetExposure(1.0f);
  scene_->SetShadowPcfRadius(2.0f);
  scene_->SetSSAOEnabled(false);
  scene_->SetTAAEnabled(false);
  scene_->SetBloomEnabled(false);
  scene_->SetGodRaysStrength(0.0f);

  // --- spawn on the surface near the origin ---------------------------------
  Respawn();

  // Clear trees near spawn (only Wood/Leaves — never carve water or terrain).
  constexpr int kClear = 14;
  for (int dx = -kClear; dx <= kClear; ++dx) {
    for (int dz = -kClear; dz <= kClear; ++dz) {
      const int top = world_.SurfaceY(spawn_x_ + dx, spawn_z_ + dz);
      for (int y = top + 1; y < World::kHeight; ++y) {
        const Block b = world_.Get(spawn_x_ + dx, y, spawn_z_ + dz);
        if (b == Block::Wood || b == Block::Leaves) {
          world_.Set(spawn_x_ + dx, y, spawn_z_ + dz, Block::Air);
        }
      }
    }
  }

  glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  LOG_INFO("Voxel") << "Seed " << world_.Seed() << "; endless terrain, " << (kRadius * 2 + 1) * (kRadius * 2 + 1)
                    << " chunks streamed around the player";
  LOG_INFO("Voxel") << "Controls: WASD move | Space jump | Space-Space or F fly | Shift/Ctrl down | 1..6 block | "
                       "Esc cursor | LMB break | RMB place";
}

void VoxelApp::Respawn() {
  // Find the nearest ocean cell: scan outward chunk-by-chunk (only the chunks
  // we actually generate get built, and we stop at the first sea chunk).
  int wx = 0, wz = 0;
  bool has_sea = false;
  constexpr int kMaxRing = 10;
  for (int r = 0; r <= kMaxRing && !has_sea; ++r) {
    for (int cdx = -r; cdx <= r && !has_sea; ++cdx) {
      for (int cdz = -r; cdz <= r && !has_sea; ++cdz) {
        if (std::max(std::abs(cdx), std::abs(cdz)) != r) {
          continue;
        }
        world_.EnsureChunk(cdx, cdz);
        const int x0 = cdx * World::kChunk;
        const int z0 = cdz * World::kChunk;
        for (int lx = 0; lx < World::kChunk && !has_sea; lx += 4) {
          for (int lz = 0; lz < World::kChunk && !has_sea; lz += 4) {
            if (world_.Get(x0 + lx, World::kSeaLevel - 1, z0 + lz) == Block::Water) {
              wx      = x0 + lx;
              wz      = z0 + lz;
              has_sea = true;
            }
          }
        }
      }
    }
  }

  // Spawn on dry land right next to that water.
  int sx = 0, sz = 0, sd = 1 << 30;
  const int kDryRange = has_sea ? 16 : 0;
  for (int dx = -kDryRange; dx <= kDryRange; ++dx) {
    for (int dz = -kDryRange; dz <= kDryRange; ++dz) {
      const int gx = wx + dx;
      const int gz = wz + dz;
      const int d  = dx * dx + dz * dz;
      if (d >= sd) {
        continue;
      }
      if (world_.SurfaceY(gx, gz) >= World::kSeaLevel - 1) {
        sd = d;
        sx = gx;
        sz = gz;
      }
    }
  }
  spawn_x_ = sx;
  spawn_z_ = sz;
  const int surf = world_.SurfaceY(spawn_x_, spawn_z_);
  position_      = glm::vec3(static_cast<float>(spawn_x_) + 0.5f, static_cast<float>(std::max(surf, 0) + 1),
                             static_cast<float>(spawn_z_) + 0.5f);
  velocity_      = glm::vec3(0.0f);
  // Look toward the ocean (defaults to +X/-Z if no sea was found nearby).
  yaw_ = has_sea ? glm::degrees(std::atan2(static_cast<float>(wx - spawn_x_), -static_cast<float>(wz - spawn_z_))) : 135.0f;
  pitch_ = -4.0f;
  LOG_INFO("Voxel") << "Respawn has_sea=" << has_sea << " water=(" << wx << "," << wz << ") spawn=(" << spawn_x_
                    << "," << spawn_z_ << ") surf=" << surf;
}

void VoxelApp::RebuildChunksAround(int center_cx, int center_cz) {
  // Drop the old tiles.
  for (auto &tile : tiles_) {
    if (tile.entity.GetHandle() != entt::null && scene_->GetRegistry().valid(tile.entity.GetHandle())) {
      scene_->DestroyEntity(tile.entity);
    }
  }
  tiles_.clear();

  active_cx_ = center_cx;
  active_cz_ = center_cz;

  for (int dz = -kRadius; dz <= kRadius; ++dz) {
    for (int dx = -kRadius; dx <= kRadius; ++dx) {
      const int cx = center_cx + dx;
      const int cz = center_cz + dz;
      PrepareChunk(world_, cx, cz);  // + neighbours so face culling is exact

      std::vector<MEngine::Vertex> verts;
      std::vector<uint32_t>        idx;
      BuildChunkMesh(world_, atlas_, cx, cz, verts, idx);
      Ref<Mesh> mesh = idx.empty() ? nullptr : Mesh::Create(verts, idx);

      Entity entity = scene_->CreateEntity("Chunk");
      entity.AddComponent<MeshComponent>(mesh, chunk_material_);
      tiles_.push_back({cx, cz, entity});
    }
  }
  LOG_DEBUG("Voxel") << "Streaming chunks around (" << center_cx << "," << center_cz << ") -> " << tiles_.size()
                     << " tiles";
}

void VoxelApp::RemeshChunk(int cx, int cz) {
  for (auto &tile : tiles_) {
    if (tile.cx != cx || tile.cz != cz) {
      continue;
    }
    PrepareChunk(world_, cx, cz);
    std::vector<MEngine::Vertex> verts;
    std::vector<uint32_t>        idx;
    BuildChunkMesh(world_, atlas_, cx, cz, verts, idx);
    Ref<Mesh> mesh = idx.empty() ? nullptr : Mesh::Create(verts, idx);
    if (tile.entity.HasComponent<MeshComponent>()) {
      tile.entity.GetComponent<MeshComponent>().mesh = mesh;
    }
    return;
  }
}

bool VoxelApp::BoxHitsSolid(const glm::vec3 &pos) const {
  const float min_x = pos.x - kHalfWidth;
  const float max_x = pos.x + kHalfWidth;
  const float min_y = pos.y;
  const float max_y = pos.y + kHeight;
  const float min_z = pos.z - kHalfWidth;
  const float max_z = pos.z + kHalfWidth;

  const int x0 = static_cast<int>(std::floor(min_x + 1e-4f));
  const int x1 = static_cast<int>(std::floor(max_x - 1e-4f));
  const int y0 = static_cast<int>(std::floor(min_y + 1e-4f));
  const int y1 = static_cast<int>(std::floor(max_y - 1e-4f));
  const int z0 = static_cast<int>(std::floor(min_z + 1e-4f));
  const int z1 = static_cast<int>(std::floor(max_z - 1e-4f));

  for (int x = x0; x <= x1; ++x) {
    for (int y = y0; y <= y1; ++y) {
      for (int z = z0; z <= z1; ++z) {
        if (world_.IsSolidCollision(x, y, z)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool VoxelApp::IsGrounded() const {
  return world_.IsSolidCollision(static_cast<int>(std::floor(position_.x)),
                                 static_cast<int>(std::floor(position_.y - 0.02f)),
                                 static_cast<int>(std::floor(position_.z)));
}

VoxelApp::Pick VoxelApp::PickBlock() const {
  const glm::vec3 eye = position_ + glm::vec3(0.0f, kEye, 0.0f);
  const glm::vec3 dir = FrontFrom(yaw_, pitch_);
  constexpr float kReach = 6.0f;
  constexpr float kStep  = 0.02f;

  Pick result;
  glm::ivec3 last_air = {static_cast<int>(std::floor(eye.x)), static_cast<int>(std::floor(eye.y)),
                         static_cast<int>(std::floor(eye.z))};
  for (float t = kStep; t <= kReach; t += kStep) {
    const glm::vec3 p    = eye + dir * t;
    const glm::ivec3 cell{static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)),
                          static_cast<int>(std::floor(p.z))};
    if (world_.IsSolidCell(cell.x, cell.y, cell.z)) {
      result.hit   = true;
      result.block = cell;
      result.place = last_air;
      return result;
    }
    last_air = cell;
  }
  return result;
}

void VoxelApp::OnUpdate(float dt) {
  dt = std::min(dt, 0.05f);

  // --- camera look ----------------------------------------------------------
  if (captured_) {
    const glm::vec2 delta = MEngine::Input::GetMouseDelta();
    yaw_ += delta.x * 0.12f;
    pitch_ = std::clamp(pitch_ - delta.y * 0.12f, -89.0f, 89.0f);
  }

  // --- one-shot keys ---------------------------------------------------------
  static bool prev_esc = false, prev_f = false, prev_space = false;
  static bool prev_digit[6] = {};

  const bool esc = MEngine::Input::IsKeyPressed(GLFW_KEY_ESCAPE);
  if (esc && !prev_esc) {
    captured_ = !captured_;
    glfwSetInputMode(window_, GLFW_CURSOR, captured_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
  }
  prev_esc = esc;

  const bool fly = MEngine::Input::IsKeyPressed(GLFW_KEY_F);
  if (fly && !prev_f) {
    flying_ = !flying_;
    velocity_.y = 0.0f;
    LOG_INFO("Voxel") << (flying_ ? "Fly mode on (F)" : "Fly mode off");
  }
  prev_f = fly;

  // Double-space toggles fly (Minecraft-style).
  const bool space = MEngine::Input::IsKeyPressed(GLFW_KEY_SPACE);
  if (space && !prev_space) {
    const float now = static_cast<float>(glfwGetTime());
    if (now - last_space_ < 0.35f) {
      flying_ = !flying_;
      velocity_.y = 0.0f;
      LOG_INFO("Voxel") << (flying_ ? "Fly mode on (space-space)" : "Fly mode off");
    }
    last_space_ = now;
  }
  prev_space = space;

  for (int i = 0; i < 6; ++i) {
    const bool d = MEngine::Input::IsKeyPressed(GLFW_KEY_1 + i);
    if (d && !prev_digit[i]) {
      hotbar_index_ = i;
      LOG_INFO("Voxel") << "Selected block type " << static_cast<int>(hotbar_[i]);
    }
    prev_digit[i] = d;
  }

  // --- movement -------------------------------------------------------------
  if (captured_) {
    const glm::vec3 flat  = glm::vec3(FrontFrom(yaw_, 0.0f).x, 0.0f, FrontFrom(yaw_, 0.0f).z);
    const glm::vec3 front = glm::length2(flat) > 1e-6f ? glm::normalize(flat) : glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 right = RightFrom(yaw_);
    glm::vec3       wish{0.0f};
    if (MEngine::Input::IsKeyPressed(GLFW_KEY_W)) wish += front;
    if (MEngine::Input::IsKeyPressed(GLFW_KEY_S)) wish -= front;
    if (MEngine::Input::IsKeyPressed(GLFW_KEY_D)) wish += right;
    if (MEngine::Input::IsKeyPressed(GLFW_KEY_A)) wish -= right;
    if (glm::length2(wish) > 0.0f) {
      wish = glm::normalize(wish);
    }

    if (flying_) {
      const float speed = kFlySpeed;
      velocity_.x       = wish.x * speed;
      velocity_.z       = wish.z * speed;
      velocity_.y       = 0.0f;
      if (MEngine::Input::IsKeyPressed(GLFW_KEY_SPACE)) velocity_.y = speed;
      if (MEngine::Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT) || MEngine::Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
        velocity_.y = -speed;
      }
    } else {
      const float k = std::min(1.0f, 14.0f * dt);
      velocity_.x += (wish.x * kWalkSpeed - velocity_.x) * k;
      velocity_.z += (wish.z * kWalkSpeed - velocity_.z) * k;
      const bool grounded = IsGrounded();
      if (space && grounded) {
        velocity_.y = kJump;
      }
      velocity_.y -= kGravity * dt;
      if (velocity_.y < -50.0f) {
        velocity_.y = -50.0f;
      }
    }

    // Integrate axis-by-axis; revert an axis that ended inside a block.
    for (int pass = 0; pass < 2; ++pass) {
      for (int axis = 0; axis < 3; ++axis) {
        const float old = position_[axis];
        position_[axis] += velocity_[axis] * dt;
        if (BoxHitsSolid(position_)) {
          position_[axis] = old;
          velocity_[axis] = 0.0f;
        }
      }
    }
  }

  // Fell out of the world? back to spawn.
  if (position_.y < -30.0f) {
    Respawn();
    LOG_INFO("Voxel") << "Respawned (fell out of the world)";
  }

  // --- streaming ------------------------------------------------------------
  const int pcx = FloorDiv(static_cast<int>(std::floor(position_.x)), World::kChunk);
  const int pcz = FloorDiv(static_cast<int>(std::floor(position_.z)), World::kChunk);
  if (pcx != active_cx_ || pcz != active_cz_) {
    RebuildChunksAround(pcx, pcz);
  }

  // --- block interaction -----------------------------------------------------
  const bool lmb = MEngine::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
  const bool rmb = MEngine::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
  if (captured_) {
    const Pick pick = PickBlock();
    if (pick.hit) {
      if (lmb && !prev_lmb_) {
        world_.Set(pick.block.x, pick.block.y, pick.block.z, Block::Air);
        RemeshChunk(FloorDiv(pick.block.x, World::kChunk), FloorDiv(pick.block.z, World::kChunk));
        RemeshChunk(FloorDiv(pick.block.x - 1, World::kChunk), FloorDiv(pick.block.z, World::kChunk));
        RemeshChunk(FloorDiv(pick.block.x + 1, World::kChunk), FloorDiv(pick.block.z, World::kChunk));
        RemeshChunk(FloorDiv(pick.block.x, World::kChunk), FloorDiv(pick.block.z - 1, World::kChunk));
        RemeshChunk(FloorDiv(pick.block.x, World::kChunk), FloorDiv(pick.block.z + 1, World::kChunk));
      } else if (rmb && !prev_rmb_) {
        const auto inside = [&](int bx, int by, int bz) {
          return position_.x - kHalfWidth < bx + 1.0f && position_.x + kHalfWidth > bx &&
                 position_.y < by + 1.0f && position_.y + kHeight > by && position_.z - kHalfWidth < bz + 1.0f &&
                 position_.z + kHalfWidth > bz;
        };
        if (world_.Get(pick.place.x, pick.place.y, pick.place.z) == Block::Air &&
            !inside(pick.place.x, pick.place.y, pick.place.z)) {
          world_.Set(pick.place.x, pick.place.y, pick.place.z, hotbar_[hotbar_index_]);
          RemeshChunk(FloorDiv(pick.place.x, World::kChunk), FloorDiv(pick.place.z, World::kChunk));
        }
      }
    }
  }
  prev_lmb_ = lmb;
  prev_rmb_ = rmb;

  // --- render ---------------------------------------------------------------
  int fb_w = 0, fb_h = 0;
  glfwGetFramebufferSize(window_, &fb_w, &fb_h);
  const float aspect = (fb_h > 0) ? static_cast<float>(fb_w) / static_cast<float>(fb_h) : 16.0f / 9.0f;

  const glm::vec3 eye  = position_ + glm::vec3(0.0f, kEye, 0.0f);
  const glm::vec3 dir  = FrontFrom(yaw_, pitch_);
  const glm::mat4 view = glm::lookAt(eye, eye + dir, glm::vec3(0.0f, 1.0f, 0.0f));
  const glm::mat4 proj = glm::perspective(glm::radians(78.0f), aspect, 0.05f, 600.0f);

  scene_->RenderMeshes(view, proj, eye);

  // --- crosshair (centre-screen), no ghost preview --------------------------
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, fb_w, fb_h);
  glEnable(GL_SCISSOR_TEST);
  const auto bar = [](int x, int y, int w, int h, float r, float g, float b) {
    glScissor(x, y, w, h);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  };
  const int cx = fb_w / 2;
  const int cy = fb_h / 2;
  bar(cx - 1, cy - 7, 2, 14, 0.05f, 0.05f, 0.05f);
  bar(cx - 7, cy - 1, 14, 2, 0.05f, 0.05f, 0.05f);
  glDisable(GL_SCISSOR_TEST);
}

}  // namespace vox

::MEngine::Application *CreateApplication() {
  return new vox::VoxelApp(::MEngine::Application::GetStartupApi());
}
