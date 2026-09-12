/**
 * @file voxel_streamer.hpp
 * @brief Asynchronous chunk streaming: the Minecraft-style split between
 * "chunk build" (background threads) and "chunk render" (the render thread).
 *
 * Building a chunk used to run inside the frame loop (generate voxel data →
 * mesh it → create GPU meshes → create Scene entities, 121 of them at once),
 * which froze rendering for the whole rebuild. Here the two halves are
 * decoupled:
 *
 *   worker threads                     render thread (once per frame)
 *   ----------------                   ------------------------------
 *   generate chunk + neighbours   ──►  Drain() finished meshes
 *   snapshot (one lock)                Mesh::Create / Scene entities,
 *   mesh (no locks, no GL)             budget-limited per frame
 *
 * Only `Mesh::Create` + entity creation touch the engine/GL, so those stay on
 * the render thread; everything CPU-heavy happens off it. Jobs are ordered by
 * distance (nearest ring first) and block edits jump the queue, so what the
 * player looks at appears first.
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "render/vertex.hpp"

#include "voxel_atlas.hpp"
#include "voxel_world.hpp"

namespace vox {

/// @brief A finished CPU-side chunk mesh. The render thread turns it into GPU
/// `Mesh`es (GL objects must be created on the render thread) and Scene
/// entities. Either mesh may be empty (all-air chunk / no water).
struct ChunkMesh {
  int      cx     = 0;
  int      cz     = 0;
  uint64_t job_id = 0;  // monotonic per-chunk request id (stale-result guard)

  std::vector<MEngine::Vertex> vertices;
  std::vector<uint32_t>        indices;
  std::vector<MEngine::Vertex> water_vertices;
  std::vector<uint32_t>        water_indices;

  [[nodiscard]] bool HasLand() const { return !indices.empty(); }
  [[nodiscard]] bool HasWater() const { return !water_indices.empty(); }
};

/// @brief Owns the background chunk-loading pipeline (a small thread pool that
/// generates + meshes chunks) plus the handshake with the render thread.
/// All public methods are meant to be called from the render thread only.
class ChunkStreamer {
 public:
  /// `worker_count <= 0` picks a sensible default (cores-1, clamped to [1,4]);
  /// the `MENGINE_VOXEL_WORKERS` env var overrides it.
  ChunkStreamer(World &world, const Atlas &atlas, int worker_count = 0);
  ~ChunkStreamer();

  ChunkStreamer(const ChunkStreamer &)            = delete;
  ChunkStreamer &operator=(const ChunkStreamer &) = delete;

  /// @brief Declares the chunk ring around the player, Minecraft style:
  ///
  ///  * `load_radius` — chunks generated + meshed (queued nearest ring first),
  ///    i.e. the area that actively streams in/out as the player moves.
  ///  * `keep_radius` — chunks that stay RESIDENT: their voxel data and their
  ///    Scene entities/meshes are kept and they keep being rendered, they are
  ///    just no longer updated. This is what makes already-explored terrain
  ///    stay on screen when the player looks back / walks back instead of
  ///    popping out of existence (must be >= load_radius + 2 so a loaded
  ///    chunk's neighbours are always available for face culling).
  ///
  /// Chunks outside `keep_radius` are returned as (cx, cz) pairs so the caller
  /// can drop their entities and free their voxel data.
  std::vector<std::pair<int, int>> SetCenter(int center_cx, int center_cz, int load_radius, int keep_radius);

  /// @brief True while (cx, cz) is tracked (actively loaded OR resident) and
  /// therefore still rendered. Results for untracked chunks are discarded.
  [[nodiscard]] bool IsTracked(int cx, int cz) const { return chunks_.count(Key(cx, cz)) != 0; }
  /// @brief True when (cx, cz) is tracked but outside the load radius (kept on
  /// screen, no longer updated).
  [[nodiscard]] bool IsResident(int cx, int cz) const;
  /// @brief True once a finished mesh for (cx, cz) has reached the render
  /// thread (used to gate player physics on loaded ground). A later remesh
  /// keeps it true — the chunk stays walkable while it is being rebuilt.
  [[nodiscard]] bool IsReady(int cx, int cz) const;

  /// @brief Re-queues (cx, cz) at the front of the queue — used after a block
  /// edit. Takes effect even when the chunk is already meshed; older in-flight
  /// results for the same chunk are discarded automatically.
  void RequestRemesh(int cx, int cz);
  /// @brief Requests a remesh of (cx, cz) and its four XZ neighbours (a block
  /// edit on a border also changes the neighbour's visible faces).
  void RequestRemeshWithNeighbours(int cx, int cz);

  /// @brief Moves finished meshes into `out` (appending; at most `max_count`).
  /// Results for chunks that are no longer wanted, or that a newer request has
  /// superseded, are dropped here.
  void Drain(std::vector<ChunkMesh> &out, int max_count);

  /// @brief Chunks currently tracked (queued, meshed or resident) — diagnostics.
  [[nodiscard]] size_t TrackedCount() const { return chunks_.size(); }
  /// @brief Tracked chunks that are only kept resident (out of the load radius).
  [[nodiscard]] size_t ResidentCount() const;
  /// @brief Jobs queued or running right now — diagnostics.
  [[nodiscard]] int InFlightCount() const { return in_flight_.load(); }
  /// @brief Worker threads actually started.
  [[nodiscard]] int WorkerCount() const { return static_cast<int>(workers_.size()); }

 private:
  struct Job {
    int      cx  = 0;
    int      cz  = 0;
    int64_t  key = 0;
    uint64_t id  = 0;
  };

  enum class Status { Queued, Meshed };
  struct ChunkState {
    int      cx       = 0;
    int      cz       = 0;
    Status   status   = Status::Queued;
    uint64_t job_id   = 0;
    bool     has_mesh = false;  // a mesh has reached the render thread
    bool     resident = false;  // tracked but outside the load radius
  };

  static int64_t Key(int cx, int cz) {
    return (static_cast<int64_t>(cx) << 32) ^ static_cast<int64_t>(static_cast<uint32_t>(cz));
  }

  void WorkerLoop();
  /// @brief Queues a mesh job and records it as the chunk's newest request.
  void QueueMesh(int cx, int cz, bool jump_queue);
  /// @brief Marks a key as no longer wanted (workers skip its queued job).
  void Cancel(int64_t key);

  World       &world_;
  const Atlas &atlas_;

  // --- job queue (guarded by queue_mutex_) ---------------------------------
  std::mutex                  queue_mutex_;
  std::condition_variable     queue_cv_;
  std::deque<Job>             queue_;
  std::unordered_set<int64_t> cancelled_;  // keys dropped by SetCenter
  bool                        stop_ = false;

  // --- finished meshes (guarded by result_mutex_) ---------------------------
  std::mutex              result_mutex_;
  std::condition_variable result_space_cv_;
  std::deque<ChunkMesh>   results_;

  // --- render-thread bookkeeping (never touched by workers) -----------------
  std::unordered_map<int64_t, ChunkState> chunks_;  // wanted set + mesh status
  uint64_t                                next_id_ = 1;

  std::atomic<int>         in_flight_{0};
  std::vector<std::thread> workers_;
};

}  // namespace vox
