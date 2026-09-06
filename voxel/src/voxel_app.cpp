#include "voxel_app.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/input.hpp"
#include "core/logger.hpp"
#include "render/asset_manager.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/texture.hpp"

namespace vox {

using MEngine::Entity;
using MEngine::Material;
using MEngine::Mesh;
using MEngine::MeshComponent;
using MEngine::Ref;
using MEngine::Texture;
using MEngine::Transform;

namespace {

constexpr int kChunksPerSide = 8;  // 128 x 128 world

glm::vec3 FrontFrom(float yaw_deg, float pitch_deg) {
  const float yaw   = glm::radians(yaw_deg);
  const float pitch = glm::radians(pitch_deg);
  const float cp    = std::cos(pitch);
  return {cp * std::sin(yaw), std::sin(pitch), -cp * std::cos(yaw)};
}

glm::vec3 RightFrom(float yaw_deg) {
  return {std::cos(glm::radians(yaw_deg)), 0.0f, std::sin(glm::radians(yaw_deg))};
}

}  // namespace

VoxelApp::VoxelApp(MEngine::GraphicsAPI api) : MEngine::Application(api) {}

VoxelApp::~VoxelApp() {}

void VoxelApp::Initialize() {
  scene_ = std::make_shared<MEngine::Scene>();
  world_ = std::make_unique<World>(kChunksPerSide, /*height=*/40, /*seed=*/1337u);

  // --- shared chunk material (one pbr material + the procedural atlas) ------
  auto atlas_texture = MEngine::CreateRef<Texture>();
  atlas_texture->SetData(const_cast<unsigned char *>(atlas_.Pixels().data()), atlas_.Width(), atlas_.Height());

  auto shader = MEngine::AssetManager::Instance().GetShader("pbr");
  chunk_material_ = MEngine::CreateRef<Material>();
  chunk_material_->SetShader(shader);
  chunk_material_->SetAlbedoMap(atlas_texture);
  chunk_material_->SetBaseColorFactor(glm::vec4(1.0f));
  chunk_material_->SetMetallicFactor(0.0f);
  chunk_material_->SetRoughnessFactor(0.9f);

  ghost_material_ = MEngine::CreateRef<Material>();
  ghost_material_->SetShader(shader);
  ghost_material_->SetBaseColorFactor(glm::vec4(0.95f, 0.92f, 0.35f, 1.0f));
  ghost_material_->SetMetallicFactor(0.0f);
  ghost_material_->SetRoughnessFactor(0.35f);

  // --- one scene entity per chunk -------------------------------------------
  const int chunks = world_->ChunkCount();
  chunk_entities_.reserve(static_cast<size_t>(chunks) * chunks);
  for (int cz = 0; cz < chunks; ++cz) {
    for (int cx = 0; cx < chunks; ++cx) {
      std::vector<MEngine::Vertex> verts;
      std::vector<uint32_t>        idx;
      BuildChunkMesh(*world_, atlas_, cx, cz, verts, idx);
      Ref<Mesh> mesh = idx.empty() ? nullptr : Mesh::Create(verts, idx);

      Entity entity = scene_->CreateEntity("Chunk");
      entity.AddComponent<MeshComponent>(mesh, chunk_material_);
      chunk_entities_.push_back(entity);
    }
  }

  // Ghost cube (placement preview); parked far below until needed.
  ghost_entity_ = scene_->CreateEntity("Ghost");
  ghost_entity_.AddComponent<Transform>(glm::vec3(0.0f, -100.0f, 0.0f));
  ghost_entity_.AddComponent<MeshComponent>(Mesh::CreateCube(1.0f), ghost_material_);
  ghost_ready_ = true;

  // --- scene lighting / post ------------------------------------------------
  // Sun coming from up-left; mild ambient so shadowed sides still read.
  scene_->GetLight().direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
  scene_->GetLight().color     = glm::vec3(1.35f);
  scene_->SetIblIntensity(0.28f);
  scene_->SetExposure(1.0f);
  scene_->SetShadowPcfRadius(2.0f);
  scene_->SetSSAOEnabled(false);
  scene_->SetTAAEnabled(false);
  scene_->SetBloomEnabled(false);
  scene_->SetGodRaysStrength(0.0f);

  // --- spawn on the surface near the middle ---------------------------------
  const int center = world_->SizeXZ() / 2;
  const int surf   = world_->SurfaceY(center, center);
  const float px   = static_cast<float>(center) + 0.5f;
  const float pz   = static_cast<float>(center) + 0.5f;
  const float py   = static_cast<float>(std::max(surf, 0) + 1);
  position_        = glm::vec3(px, py, pz);
  yaw_             = 135.0f;   // look back toward the world interior
  pitch_           = -8.0f;

  glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  LOG_INFO("Voxel") << "World " << world_->SizeXZ() << "x" << world_->Height() << "x" << world_->SizeXZ()
                    << " ready (" << chunks * chunks << " chunks)";
  LOG_INFO("Voxel") << "Controls: WASD move | Space jump/up | Shift down | F fly | 1..6 block | Esc cursor | "
                       "LMB break | RMB place";
}

void VoxelApp::RemeshChunk(int cx, int cz) {
  const int chunks = world_->ChunkCount();
  if (cx < 0 || cz < 0 || cx >= chunks || cz >= chunks) {
    return;
  }
  const size_t index = static_cast<size_t>(cz) * chunks + static_cast<size_t>(cx);
  if (index >= chunk_entities_.size()) {
    return;
  }
  std::vector<MEngine::Vertex> verts;
  std::vector<uint32_t>        idx;
  BuildChunkMesh(*world_, atlas_, cx, cz, verts, idx);
  Ref<Mesh> mesh = idx.empty() ? nullptr : Mesh::Create(verts, idx);
  if (chunk_entities_[index].HasComponent<MeshComponent>()) {
    chunk_entities_[index].GetComponent<MeshComponent>().mesh = mesh;
  }
}

void VoxelApp::RemeshAround(int x, int z) {
  const int chunks = world_->ChunkCount();
  const int cx     = std::clamp(x / World::kChunk, 0, chunks - 1);
  const int cz     = std::clamp(z / World::kChunk, 0, chunks - 1);
  for (int dz = -1; dz <= 1; ++dz) {
    for (int dx = -1; dx <= 1; ++dx) {
      RemeshChunk(cx + dx, cz + dz);
    }
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
    for (int z = z0; z <= z1; ++z) {
      for (int y = y0; y <= y1; ++y) {
        if (world_->IsSolidCell(x, y, z)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool VoxelApp::IsGrounded() const {
  return world_->IsSolidCell(static_cast<int>(std::floor(position_.x)),
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
    const glm::vec3 p = eye + dir * t;
    const glm::ivec3 cell{static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)),
                          static_cast<int>(std::floor(p.z))};
    if (world_->IsSolidCell(cell.x, cell.y, cell.z)) {
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

  // --- camera look (mouse) --------------------------------------------------
  if (captured_) {
    const glm::vec2 delta = MEngine::Input::GetMouseDelta();
    yaw_ += delta.x * 0.12f;
    pitch_ = std::clamp(pitch_ - delta.y * 0.12f, -89.0f, 89.0f);
  }

  // --- one-shot keys (edge detection) ---------------------------------------
  static bool prev_esc = false, prev_f = false, prev_digit[6] = {};
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
    LOG_INFO("Voxel") << (flying_ ? "Fly mode on" : "Fly mode off");
  }
  prev_f = fly;

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
    const glm::vec3 front_flat = glm::normalize(glm::vec3(FrontFrom(yaw_, 0.0f).x, 0.0f, FrontFrom(yaw_, 0.0f).z));
    const glm::vec3 right      = RightFrom(yaw_);
    glm::vec3 wish{0.0f};
    if (MEngine::Input::IsKeyPressed(GLFW_KEY_W)) wish += front_flat;
    if (MEngine::Input::IsKeyPressed(GLFW_KEY_S)) wish -= front_flat;
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
      // horizontal velocity approaches the walk speed
      const float target_x = wish.x * kWalkSpeed;
      const float target_z = wish.z * kWalkSpeed;
      constexpr float accel = 12.0f;  // 1/s
      const float k = std::min(1.0f, accel * dt);
      velocity_.x += (target_x - velocity_.x) * k;
      velocity_.z += (target_z - velocity_.z) * k;

      const bool grounded = IsGrounded();
      if (MEngine::Input::IsKeyPressed(GLFW_KEY_SPACE) && grounded) {
        velocity_.y = kJump;
      }
      velocity_.y -= kGravity * dt;
      if (velocity_.y < -50.0f) velocity_.y = -50.0f;
    }

    // Integrate one axis at a time; if we end up inside a block, revert that
    // axis and cancel its velocity (keeps a collision-free, wall-sliding AABB).
    const int axes[3] = {0, 1, 2};
    for (int pass = 0; pass < 2; ++pass) {
      for (int axis : axes) {
        const float old = position_[axis];
        position_[axis] += velocity_[axis] * dt;
        if (BoxHitsSolid(position_)) {
          position_[axis] = old;
          velocity_[axis] = 0.0f;
        }
      }
    }
  }

  // Fell off the world edge? respawn.
  if (position_.y < -30.0f) {
    const int center = world_->SizeXZ() / 2;
    position_        = glm::vec3(static_cast<float>(center) + 0.5f, static_cast<float>(world_->SurfaceY(center, center) + 1),
                                 static_cast<float>(center) + 0.5f);
    velocity_        = glm::vec3(0.0f);
    LOG_INFO("Voxel") << "Respawned (fell out of the world)";
  }

  // --- block interaction (edge clicks) ---------------------------------------
  const bool lmb = MEngine::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
  const bool rmb = MEngine::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
  if (captured_) {
    const Pick pick = PickBlock();
    if (pick.hit) {
      if (lmb && !prev_lmb_) {
        world_->Set(pick.block.x, pick.block.y, pick.block.z, Block::Air);
        RemeshAround(pick.block.x, pick.block.z);
      } else if (rmb && !prev_rmb_) {
        // refuse to place the block inside the player's AABB
        const auto solid_cell = [&](int bx, int by, int bz) {
          const float pmin_x = position_.x - kHalfWidth, pmax_x = position_.x + kHalfWidth;
          const float pmin_y = position_.y, pmax_y = position_.y + kHeight;
          const float pmin_z = position_.z - kHalfWidth, pmax_z = position_.z + kHalfWidth;
          const float bmin_x = bx, bmax_x = bx + 1.0f;
          const float bmin_y = by, bmax_y = by + 1.0f;
          const float bmin_z = bz, bmax_z = bz + 1.0f;
          return pmin_x < bmax_x && pmax_x > bmin_x && pmin_y < bmax_y && pmax_y > bmin_y && pmin_z < bmax_z &&
                 pmax_z > bmin_z;
        };
        const bool inside_player = solid_cell(pick.place.x, pick.place.y, pick.place.z);
        if (world_->Get(pick.place.x, pick.place.y, pick.place.z) == Block::Air && !inside_player) {
          world_->Set(pick.place.x, pick.place.y, pick.place.z, hotbar_[hotbar_index_]);
          RemeshAround(pick.place.x, pick.place.z);
        }
      }
    }

    // ghost preview = the placement cell (hidden when no valid neighbour).
    if (ghost_ready_) {
      glm::vec3 ghost_pos(0.0f, -100.0f, 0.0f);
      if (pick.hit && world_->Get(pick.place.x, pick.place.y, pick.place.z) == Block::Air) {
        ghost_pos = glm::vec3(pick.place.x + 0.5f, pick.place.y + 0.5f, pick.place.z + 0.5f);
      }
      if (ghost_entity_.HasComponent<Transform>()) {
        ghost_entity_.GetComponent<Transform>().translation = ghost_pos;
      }
    }
  }
  prev_lmb_ = lmb;
  prev_rmb_ = rmb;

  // --- render ---------------------------------------------------------------
  int fb_w = 0, fb_h = 0;
  glfwGetFramebufferSize(window_, &fb_w, &fb_h);
  const float aspect = (fb_h > 0) ? static_cast<float>(fb_w) / static_cast<float>(fb_h) : 16.0f / 9.0f;

  const glm::vec3 eye = position_ + glm::vec3(0.0f, kEye, 0.0f);
  const glm::vec3 dir = FrontFrom(yaw_, pitch_);
  const glm::mat4 view = glm::lookAt(eye, eye + dir, glm::vec3(0.0f, 1.0f, 0.0f));
  const glm::mat4 proj = glm::perspective(glm::radians(78.0f), aspect, 0.05f, 600.0f);

  scene_->RenderMeshes(view, proj, eye);
}

}  // namespace vox

::MEngine::Application *CreateApplication() {
  return new vox::VoxelApp(::MEngine::Application::GetStartupApi());
}
