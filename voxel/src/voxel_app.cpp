#include "voxel_app.hpp"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <utility>

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
  char  *buffer = nullptr;
  size_t len    = 0;
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

/// @brief Reads a float from an environment variable (0 when unset / not a
/// number). Uses the MSVC-safe `_dupenv_s` on Windows.
float EnvFloat(const char *name) {
#if defined(_WIN32)
  char  *buffer = nullptr;
  size_t len    = 0;
  float  value  = 0.0f;
  if (_dupenv_s(&buffer, &len, name) == 0 && buffer != nullptr) {
    if (buffer[0] != '\0') {
      value = static_cast<float>(std::atof(buffer));
    }
    free(buffer);
  }
  return value;
#else
  const char *env = std::getenv(name);
  return (env != nullptr && env[0] != '\0') ? static_cast<float>(std::atof(env)) : 0.0f;
#endif
}

glm::vec3 FrontFrom(float yaw_deg, float pitch_deg) {
  const float yaw   = glm::radians(yaw_deg);
  const float pitch = glm::radians(pitch_deg);
  const float cp    = std::cos(pitch);
  return {cp * std::sin(yaw), std::sin(pitch), -cp * std::cos(yaw)};
}

glm::vec3 RightFrom(float yaw_deg) { return {std::cos(glm::radians(yaw_deg)), 0.0f, std::sin(glm::radians(yaw_deg))}; }

int FloorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }

/// @brief Key of a chunk in the app's tile map (mirrors the world's layout).
int64_t ChunkKey(int cx, int cz) {
  return (static_cast<int64_t>(cx) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(cz));
}

/// @brief Milliseconds elapsed since `start`.
float ElapsedMs(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
}

// Chunk streaming runs inside the frame loop, under ONE shared time budget that
// covers both the GPU upload of newly meshed chunks and the teardown of chunks
// that left the radius. Every frame always makes progress (at least one item)
// and then stops as soon as the budget is spent, so terrain streaming can never
// stall frames the way the old synchronous 121-chunk rebuild did (it took ~1 s
// and froze rendering for the whole border crossing).
constexpr float kStreamBudgetMs     = 4.0f;
constexpr int   kMaxUploadsPerFrame = 3;
// Teardown is dominated by Scene bookkeeping/logging, so retired chunks are
// stripped + parked off the frame path; keep it to a few tiles per frame.
constexpr int kMaxRetiresPerFrame = 8;
// Upper bound on parked entities (each is invisible and tiny).
constexpr size_t kMaxFreeTiles = 256;

/// @brief Draws a full-screen translucent tint. Used for the underwater view
/// so, when the camera is inside a water cell, the whole screen gets a blue
/// "water column" tint (and looking up at the surface isn't just clear sky).
void DrawFullscreenTint(float r, float g, float b, float a) {
  static GLuint program = 0;
  static GLuint vao     = 0;
  if (program == 0) {
    const char *vs =
        "#version 460 core\n"
        "void main() {\n"
        "  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
        "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
        "}\n";
    const char *fs =
        "#version 460 core\n"
        "uniform vec4 u_color;\n"
        "out vec4 FragColor;\n"
        "void main() { FragColor = u_color; }\n";
    const auto compile = [](GLenum type, const char *src) {
      GLuint s = glCreateShader(type);
      glShaderSource(s, 1, &src, nullptr);
      glCompileShader(s);
      GLint ok = GL_FALSE;
      glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
      if (!ok) {
        char log[1024] = {0};
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        LOG_ERROR("Voxel") << "Underwater overlay shader failed: " << log;
      }
      return s;
    };
    const GLuint vs_id = compile(GL_VERTEX_SHADER, vs);
    const GLuint fs_id = compile(GL_FRAGMENT_SHADER, fs);
    program            = glCreateProgram();
    glAttachShader(program, vs_id);
    glAttachShader(program, fs_id);
    glLinkProgram(program);
    glDeleteShader(vs_id);
    glDeleteShader(fs_id);
    glGenVertexArrays(1, &vao);
  }
  glUseProgram(program);
  glUniform4f(glGetUniformLocation(program, "u_color"), r, g, b, a);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glUseProgram(0);
}

}  // namespace

VoxelApp::VoxelApp(MEngine::GraphicsAPI api) : MEngine::Application(api), world_(StartupSeed()) {}

VoxelApp::~VoxelApp() {}

void VoxelApp::Initialize() {
  scene_ = std::make_shared<MEngine::Scene>();

  // --- shared chunk material (one pbr material + the procedural atlas) ------
  auto atlas_texture = MEngine::CreateRef<Texture>();
  atlas_texture->SetData(const_cast<unsigned char *>(atlas_.Pixels().data()), atlas_.Width(), atlas_.Height());

  auto shader     = MEngine::AssetManager::Instance().GetShader("pbr");
  chunk_material_ = MEngine::CreateRef<MEngine::Material>();
  chunk_material_->SetShader(shader);
  chunk_material_->SetAlbedoMap(atlas_texture);
  chunk_material_->SetBaseColorFactor(glm::vec4(1.0f));
  chunk_material_->SetMetallicFactor(0.0f);
  chunk_material_->SetRoughnessFactor(0.9f);

  // --- shared translucent water material ------------------------------------
  // Same albedo atlas (water tile), drawn with alpha blending after the opaque
  // pass, double-sided so the surface is visible from above AND below water.
  water_material_ = MEngine::CreateRef<MEngine::Material>();
  water_material_->SetShader(shader);
  water_material_->SetAlbedoMap(atlas_texture);
  water_material_->SetBaseColorFactor(glm::vec4(1.0f, 1.0f, 1.0f, 0.72f));  // opacity
  water_material_->SetMetallicFactor(0.0f);
  water_material_->SetRoughnessFactor(0.1f);
  water_material_->SetTranslucent(true);
  water_material_->SetCullMode(MEngine::CullMode::None);

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

  // Optional debug camera override (unattended captures / verification):
  // MENGINE_VOXEL_DEBUG_CAM="x,y,z,yaw,pitch" teleports the eye after spawn.
  const char *debug_cam = nullptr;
#if defined(_WIN32)
  {
    char  *buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, "MENGINE_VOXEL_DEBUG_CAM") == 0) {
      debug_cam = buf;  // freed below after parsing
    }
#else
  debug_cam = std::getenv("MENGINE_VOXEL_DEBUG_CAM");
#endif
    if (debug_cam != nullptr && debug_cam[0] != '\0') {
      float       v[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
      int         n    = 0;
      const char *p    = debug_cam;
      while (n < 5 && *p != '\0') {
        char *end = nullptr;
        v[n]      = std::strtof(p, &end);
        if (end == p) {
          break;  // not a number
        }
        ++n;
        p = end;
        if (*p == ',') {
          ++p;
        }
      }
      if (n == 5) {
        position_    = glm::vec3(v[0], v[1], v[2]);
        yaw_         = v[3];
        pitch_       = v[4];
        const int sx = static_cast<int>(std::floor(v[0]));
        const int sy = static_cast<int>(std::floor(v[1]));
        const int sz = static_cast<int>(std::floor(v[2]));
        LOG_INFO("Voxel") << "Debug camera -> " << v[0] << "," << v[1] << "," << v[2] << " yaw " << v[3] << " pitch "
                          << v[4] << " surf=" << world_.SurfaceY(sx, sz)
                          << " eyeCell=" << static_cast<int>(world_.Get(sx, sy, sz));
      }
    }
#if defined(_WIN32)
    free(const_cast<char *>(debug_cam));
  }
#endif

  glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  // Optional unattended-test hook: fly straight forward so chunk streaming can
  // be measured headlessly (MENGINE_VOXEL_AUTOWALK=<blocks/s>).
  autowalk_speed_ = EnvFloat("MENGINE_VOXEL_AUTOWALK");
  if (autowalk_speed_ > 0.0f) {
    flying_ = true;
    LOG_INFO("Voxel") << "Autowalk " << autowalk_speed_ << " blocks/s (unattended streaming test)";
  }

  // Resident radius: already-loaded chunks inside it stay rendered (and keep
  // their data + meshes) after the player walks away, Minecraft style.
  const float keep_env = EnvFloat("MENGINE_VOXEL_KEEP");
  if (keep_env > 0.0f) {
    keep_radius_ = std::clamp(static_cast<int>(keep_env), kMinKeepRadius, kMaxKeepRadius);
  }

  // --- background chunk streaming -------------------------------------------
  // Chunk generation + meshing happen on worker threads from here on; this
  // frame loop only uploads a budget-limited number of finished meshes.
  streamer_ = std::make_unique<ChunkStreamer>(world_, atlas_);

  const int spawn_cx = FloorDiv(static_cast<int>(std::floor(position_.x)), World::kChunk);
  const int spawn_cz = FloorDiv(static_cast<int>(std::floor(position_.z)), World::kChunk);
  PrewarmSpawn(spawn_cx, spawn_cz);

  LOG_INFO("Voxel") << "Seed " << world_.Seed() << "; endless terrain, " << (kRadius * 2 + 1) * (kRadius * 2 + 1)
                    << " chunks actively streamed around the player, "
                    << (keep_radius_ * 2 + 1) * (keep_radius_ * 2 + 1) << " kept resident (radius " << kRadius << "/"
                    << keep_radius_ << ")";
  LOG_INFO("Voxel") << "Controls: WASD move | Space jump | Space-Space or F fly | Shift/Ctrl down | 1..6 block | "
                       "Esc cursor | LMB break | RMB place";
}

void VoxelApp::Respawn() {
  // Find the nearest ocean cell by sampling the pure heightmap: no chunk is
  // generated for the search, so spawning / respawning stays instant (the
  // chunks are streamed in by the background workers afterwards).
  int           wx = 0, wz = 0;
  bool          has_sea  = false;
  constexpr int kMaxRing = 10;
  for (int r = 0; r <= kMaxRing && !has_sea; ++r) {
    for (int cdx = -r; cdx <= r && !has_sea; ++cdx) {
      for (int cdz = -r; cdz <= r && !has_sea; ++cdz) {
        if (std::max(std::abs(cdx), std::abs(cdz)) != r) {
          continue;
        }
        const int x0 = cdx * World::kChunk;
        const int z0 = cdz * World::kChunk;
        for (int lx = 0; lx < World::kChunk && !has_sea; lx += 4) {
          for (int lz = 0; lz < World::kChunk && !has_sea; lz += 4) {
            if (world_.TerrainHeight(x0 + lx, z0 + lz) < World::kSeaLevel) {
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
  int       sx = 0, sz = 0, sd = 1 << 30;
  const int kDryRange = has_sea ? 16 : 0;
  for (int dx = -kDryRange; dx <= kDryRange; ++dx) {
    for (int dz = -kDryRange; dz <= kDryRange; ++dz) {
      const int gx = wx + dx;
      const int gz = wz + dz;
      const int d  = dx * dx + dz * dz;
      if (d >= sd) {
        continue;
      }
      if (world_.TerrainHeight(gx, gz) >= World::kSeaLevel) {  // dry = above the water line
        sd = d;
        sx = gx;
        sz = gz;
      }
    }
  }
  spawn_x_       = sx;
  spawn_z_       = sz;
  const int surf = world_.TerrainHeight(spawn_x_, spawn_z_) - 1;
  position_      = glm::vec3(static_cast<float>(spawn_x_) + 0.5f, static_cast<float>(std::max(surf, 0) + 1),
                             static_cast<float>(spawn_z_) + 0.5f);
  velocity_      = glm::vec3(0.0f);
  // Look toward the ocean (defaults to +X/-Z if no sea was found nearby).
  yaw_   = has_sea ? glm::degrees(std::atan2(static_cast<float>(wx - spawn_x_), -static_cast<float>(wz - spawn_z_)))
                   : 135.0f;
  pitch_ = -4.0f;
  LOG_INFO("Voxel") << "Respawn has_sea=" << has_sea << " water=(" << wx << "," << wz << ") spawn=(" << spawn_x_ << ","
                    << spawn_z_ << ") surf=" << surf;
}

void VoxelApp::UpdateStreaming(int center_cx, int center_cz) {
  if (center_cx == active_cx_ && center_cz == active_cz_) {
    return;
  }
  active_cx_ = center_cx;
  active_cz_ = center_cz;

  // The streamer owns the ring bookkeeping: it queues everything inside the
  // load radius (nearest ring first) on its worker threads, keeps the chunks in
  // the larger keep radius resident, and hands back only the chunks that fell
  // outside the keep ring. No generation, meshing or GPU work happens here.
  const std::vector<std::pair<int, int>> dropped = streamer_->SetCenter(center_cx, center_cz, kRadius, keep_radius_);
  for (const auto &[cx, cz] : dropped) {
    // Outside the keep ring: it is no longer rendered, so retire the entities
    // (torn down a few per frame) and free its voxel data. A revisit far away
    // regenerates it from the seed (losing edits made out there).
    RetireTile(ChunkKey(cx, cz));
    world_.UnloadChunk(cx, cz);
  }
}

void VoxelApp::UploadFinishedChunks(std::chrono::steady_clock::time_point deadline, int max_count) {
  // Cheap: this only moves finished CPU meshes out of the worker queue.
  streamer_->Drain(upload_queue_, max_count * 2);

  int    applied  = 0;
  size_t consumed = 0;
  while (consumed < upload_queue_.size() && applied < max_count) {
    ApplyChunkMesh(std::move(upload_queue_[consumed]));
    ++consumed;
    ++applied;
    // Always land at least one chunk, then stop once this frame's streaming
    // budget is spent: creating a chunk mesh is a GPU upload that the driver
    // may stall on, so the burst is spread over several frames.
    if (std::chrono::steady_clock::now() >= deadline) {
      break;
    }
  }
  if (consumed > 0) {
    upload_queue_.erase(upload_queue_.begin(), upload_queue_.begin() + static_cast<ptrdiff_t>(consumed));
  }
}

void VoxelApp::ApplyChunkMesh(ChunkMesh &&mesh) {
  if (!streamer_->IsTracked(mesh.cx, mesh.cz)) {
    return;  // the player walked away while this chunk was being built
  }
  const int64_t key = ChunkKey(mesh.cx, mesh.cz);

  // Reuse this chunk's entities when it is already on screen, recycle a tile
  // that is queued for parking when the player just walked back, otherwise take
  // an entity out of the pool (and only create a real entity if it is empty).
  ChunkTile  tile;
  const auto existing = tiles_.find(key);
  if (existing != tiles_.end()) {
    tile = existing->second;
  } else if (!TakeRetiredTile(key, tile)) {
    tile = TakeFreeTile();
  }

  // Attaches `gpu` to an entity, reusing the entity when it exists (this is
  // what keeps streaming cheap: no Scene create/destroy per chunk movement).
  const auto attach = [&](Entity &entity, const Ref<Mesh> &gpu, const Ref<MEngine::Material> &material,
                          const char *name) {
    if (!IsAlive(entity)) {
      entity = scene_->CreateEntity(name);
      entity.AddComponent<MeshComponent>(gpu, material);
    } else if (entity.HasComponent<MeshComponent>()) {
      entity.GetComponent<MeshComponent>().mesh = gpu;  // releases the previous mesh
    } else {
      entity.AddComponent<MeshComponent>(gpu, material);
    }
  };

  // Opaque terrain.
  if (mesh.HasLand()) {
    attach(tile.entity, Mesh::Create(mesh.vertices, mesh.indices), chunk_material_, "Chunk");
  } else {
    DetachMesh(tile.entity);
  }

  // Translucent water, same lifecycle on its own entity.
  if (mesh.HasWater()) {
    attach(tile.water, Mesh::Create(mesh.water_vertices, mesh.water_indices), water_material_, "ChunkWater");
  } else {
    DetachMesh(tile.water);
  }

  if (IsAlive(tile.entity) || IsAlive(tile.water)) {
    tiles_[key] = tile;
  } else {
    tiles_.erase(key);  // fully empty chunk: keep no bookkeeping for it
  }
}

void VoxelApp::DetachMesh(Entity entity) {
  if (IsAlive(entity) && entity.HasComponent<MeshComponent>()) {
    entity.RemoveComponent<MeshComponent>();  // releases the chunk's GPU buffers
  }
}

void VoxelApp::RetireTile(int64_t key) {
  const auto it = tiles_.find(key);
  if (it == tiles_.end()) {
    return;
  }
  retired_.emplace_back(key, it->second);
  tiles_.erase(it);
}

bool VoxelApp::TakeRetiredTile(int64_t key, ChunkTile &out) {
  for (auto it = retired_.begin(); it != retired_.end(); ++it) {
    if (it->first == key) {
      out = it->second;
      retired_.erase(it);
      return true;
    }
  }
  return false;
}

VoxelApp::ChunkTile VoxelApp::TakeFreeTile() {
  if (free_tiles_.empty()) {
    return ChunkTile{};
  }
  const ChunkTile tile = free_tiles_.back();
  free_tiles_.pop_back();
  return tile;
}

void VoxelApp::DrainRetiredTiles(std::chrono::steady_clock::time_point deadline, int max_count) {
  int parked = 0;
  while (!retired_.empty() && parked < max_count) {
    ChunkTile &tile = retired_.front().second;
    DetachMesh(tile.entity);  // cheap (~0.1 ms/mesh) compared to entity destroy
    DetachMesh(tile.water);
    if (free_tiles_.size() < kMaxFreeTiles) {
      free_tiles_.push_back(tile);
    } else {
      DestroyTileEntities(tile);  // pool full: actually free the entities
    }
    retired_.pop_front();
    ++parked;
    if (std::chrono::steady_clock::now() >= deadline) {
      break;
    }
  }
}

void VoxelApp::DestroyTileEntities(ChunkTile &tile) {
  if (IsAlive(tile.entity)) {
    scene_->DestroyEntity(tile.entity);
  }
  if (IsAlive(tile.water)) {
    scene_->DestroyEntity(tile.water);
  }
  tile.entity = Entity{};
  tile.water  = Entity{};
}

bool VoxelApp::IsAlive(Entity entity) const {
  const entt::entity handle = entity.GetHandle();
  return handle != entt::null && scene_->GetRegistry().valid(handle);
}

bool VoxelApp::GroundReady(int cx, int cz) const {
  for (int dz = -1; dz <= 1; ++dz) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (!streamer_->IsReady(cx + dx, cz + dz)) {
        return false;
      }
    }
  }
  return true;
}

void VoxelApp::PrewarmSpawn(int center_cx, int center_cz) {
  UpdateStreaming(center_cx, center_cz);
  const auto stall_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!GroundReady(center_cx, center_cz) && std::chrono::steady_clock::now() < stall_deadline) {
    // Same path the frame loop uses, just with a bigger budget: this is the
    // equivalent of a "Loading world" screen and only runs once at startup.
    UploadFinishedChunks(std::chrono::steady_clock::now() + std::chrono::milliseconds(8), 8);
    DrainRetiredTiles(std::chrono::steady_clock::now() + std::chrono::milliseconds(8), 8);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  LOG_INFO("Voxel") << "Spawn prewarm " << (GroundReady(center_cx, center_cz) ? "ready" : "timed out") << " ("
                    << tiles_.size() << " tiles, " << world_.ChunkCount() << " chunks generated, "
                    << streamer_->WorkerCount() << " workers)";
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
  const glm::vec3 eye    = position_ + glm::vec3(0.0f, kEye, 0.0f);
  const glm::vec3 dir    = FrontFrom(yaw_, pitch_);
  constexpr float kReach = 6.0f;
  constexpr float kStep  = 0.02f;

  Pick       result;
  glm::ivec3 last_air = {static_cast<int>(std::floor(eye.x)), static_cast<int>(std::floor(eye.y)),
                         static_cast<int>(std::floor(eye.z))};
  for (float t = kStep; t <= kReach; t += kStep) {
    const glm::vec3  p = eye + dir * t;
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

  // --- chunk streaming ------------------------------------------------------
  // Workers have been generating + meshing since the last frame; here we only
  // (a) re-target the wanted ring when the player crosses a chunk border and
  // (b) upload a budget-limited number of finished meshes. Neither can block
  // the frame for more than a few milliseconds, so rendering keeps running
  // while terrain streams in.
  const int  pcx          = FloorDiv(static_cast<int>(std::floor(position_.x)), World::kChunk);
  const int  pcz          = FloorDiv(static_cast<int>(std::floor(position_.z)), World::kChunk);
  const auto stream_start = std::chrono::steady_clock::now();
  UpdateStreaming(pcx, pcz);
  const auto  deadline     = stream_start + std::chrono::milliseconds(static_cast<int>(kStreamBudgetMs));
  const float update_ms    = ElapsedMs(stream_start);
  const auto  upload_start = std::chrono::steady_clock::now();
  UploadFinishedChunks(deadline, kMaxUploadsPerFrame);
  const float upload_ms    = ElapsedMs(upload_start);
  const auto  retire_start = std::chrono::steady_clock::now();
  DrainRetiredTiles(deadline, kMaxRetiresPerFrame);
  const float retire_ms = ElapsedMs(retire_start);
  const float stream_ms = update_ms + upload_ms + retire_ms;

  // Streaming diagnostics (same cadence as the engine's [RenderStats]): proves
  // the per-frame cost stays bounded while chunks stream in/out.
  static int   stream_frames    = 0;
  static float stream_worst_up  = 0.0f;
  static float stream_worst_lo  = 0.0f;
  static float stream_worst_ret = 0.0f;
  static float stream_total_ms  = 0.0f;
  stream_worst_up               = std::max(stream_worst_up, update_ms);
  stream_worst_lo               = std::max(stream_worst_lo, upload_ms);
  stream_worst_ret              = std::max(stream_worst_ret, retire_ms);
  stream_total_ms += stream_ms;
  if (++stream_frames % 120 == 0) {
    LOG_INFO("Voxel") << "Streaming: " << tiles_.size() << " tiles/" << streamer_->TrackedCount() << " tracked ("
                      << streamer_->ResidentCount() << " resident), " << streamer_->InFlightCount()
                      << " jobs in flight, avg " << (stream_total_ms / 120.0f) << " ms/frame, worst update "
                      << stream_worst_up << " upload " << stream_worst_lo << " retire " << stream_worst_ret << " ms";
    stream_worst_up  = 0.0f;
    stream_worst_lo  = 0.0f;
    stream_worst_ret = 0.0f;
    stream_total_ms  = 0.0f;
  }

  // Until the chunks right under the player exist (startup, respawn or a debug
  // teleport) the player is held in place instead of falling through the void.
  const bool ground_ready = GroundReady(pcx, pcz);

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
    flying_     = !flying_;
    velocity_.y = 0.0f;
    LOG_INFO("Voxel") << (flying_ ? "Fly mode on (F)" : "Fly mode off");
  }
  prev_f = fly;

  // Double-space toggles fly (Minecraft-style).
  const bool space = MEngine::Input::IsKeyPressed(GLFW_KEY_SPACE);
  if (space && !prev_space) {
    const float now = static_cast<float>(glfwGetTime());
    if (now - last_space_ < 0.35f) {
      flying_     = !flying_;
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
  if (captured_ && ground_ready) {
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

    if (autowalk_speed_ > 0.0f) {
      // Unattended streaming test: fly along +X so the camera (set by
      // MENGINE_VOXEL_DEBUG_CAM) can be aimed independently — e.g. looking back
      // at the terrain just flown over to verify resident chunks.
      wish = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    if (flying_) {
      const float speed = (autowalk_speed_ > 0.0f) ? autowalk_speed_ : kFlySpeed;
      velocity_.x       = wish.x * speed;
      velocity_.z       = wish.z * speed;
      velocity_.y       = 0.0f;
      if (MEngine::Input::IsKeyPressed(GLFW_KEY_SPACE)) velocity_.y = speed;
      if (MEngine::Input::IsKeyPressed(GLFW_KEY_LEFT_SHIFT) || MEngine::Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
        velocity_.y = -speed;
      }
    } else {
      const float speed = (autowalk_speed_ > 0.0f) ? autowalk_speed_ : kWalkSpeed;
      const float k     = std::min(1.0f, 14.0f * dt);
      velocity_.x += (wish.x * speed - velocity_.x) * k;
      velocity_.z += (wish.z * speed - velocity_.z) * k;
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

  // Fell out of the world? back to spawn (the reloaded chunks re-gate physics
  // through `ground_ready` until they arrive).
  if (ground_ready && position_.y < -30.0f) {
    Respawn();
    LOG_INFO("Voxel") << "Respawned (fell out of the world)";
  }

  // --- block interaction -----------------------------------------------------
  const bool lmb = MEngine::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
  const bool rmb = MEngine::Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
  if (captured_ && ground_ready) {
    const Pick pick = PickBlock();
    if (pick.hit) {
      // Edits change the world data immediately (physics/picking see them right
      // away); the affected chunk + neighbours are re-meshed on a worker thread
      // and the new mesh is uploaded on a following frame.
      if (lmb && !prev_lmb_) {
        world_.Set(pick.block.x, pick.block.y, pick.block.z, Block::Air);
        streamer_->RequestRemeshWithNeighbours(FloorDiv(pick.block.x, World::kChunk),
                                               FloorDiv(pick.block.z, World::kChunk));
      } else if (rmb && !prev_rmb_) {
        const auto inside = [&](int bx, int by, int bz) {
          return position_.x - kHalfWidth < bx + 1.0f && position_.x + kHalfWidth > bx && position_.y < by + 1.0f &&
                 position_.y + kHeight > by && position_.z - kHalfWidth < bz + 1.0f && position_.z + kHalfWidth > bz;
        };
        if (world_.Get(pick.place.x, pick.place.y, pick.place.z) == Block::Air &&
            !inside(pick.place.x, pick.place.y, pick.place.z)) {
          world_.Set(pick.place.x, pick.place.y, pick.place.z, hotbar_[hotbar_index_]);
          streamer_->RequestRemeshWithNeighbours(FloorDiv(pick.place.x, World::kChunk),
                                                 FloorDiv(pick.place.z, World::kChunk));
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

  // --- underwater tint + crosshair (composited onto the default FB) ---------
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, fb_w, fb_h);

  // When the eye is inside a water cell, wash the whole screen in blue so the
  // view reads as "underwater" (looking up at the surface is no longer bare
  // sky, and distant terrain gets a depth-blue haze). Strength grows with how
  // far below the surface the camera sits.
  const glm::ivec3 eye_cell{static_cast<int>(std::floor(eye.x)), static_cast<int>(std::floor(eye.y)),
                            static_cast<int>(std::floor(eye.z))};
  if (world_.Get(eye_cell.x, eye_cell.y, eye_cell.z) == Block::Water) {
    const float depth = std::max(0.0f, static_cast<float>(World::kSeaLevel) - eye.y);
    DrawFullscreenTint(0.02f, 0.32f, 0.58f, std::min(0.62f, 0.32f + depth * 0.06f));
  }

  // --- crosshair (centre-screen), no ghost preview --------------------------
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

::MEngine::Application *CreateApplication() { return new vox::VoxelApp(::MEngine::Application::GetStartupApi()); }
