#include "voxel_streamer.hpp"

#include <algorithm>
#include <cstdlib>

#include "core/logger.hpp"

namespace vox {

namespace {

/// Upper bound on finished-but-not-yet-uploaded meshes: keeps the worker pool
/// from building the whole world ahead of the frame uploader (bounded latency
/// and memory: a chunk mesh is a few hundred KB).
constexpr int kMaxPendingResults = 24;

int DefaultWorkerCount() {
  const unsigned int cores = std::max(1u, std::thread::hardware_concurrency());
  return std::clamp(static_cast<int>(cores) - 1, 1, 4);
}

/// `MENGINE_VOXEL_WORKERS=<n>` (n >= 1) overrides the worker count; anything
/// else (unset, 0, garbage) falls back to the computed default.
int ConfiguredWorkerCount() {
#if defined(_WIN32)
  char  *buffer = nullptr;
  size_t len    = 0;
  if (_dupenv_s(&buffer, &len, "MENGINE_VOXEL_WORKERS") == 0 && buffer != nullptr && buffer[0] != '\0') {
    const int n = std::atoi(buffer);
    free(buffer);
    if (n >= 1) {
      return n;
    }
  } else if (buffer != nullptr) {
    free(buffer);
  }
#else
  const char *env = std::getenv("MENGINE_VOXEL_WORKERS");
  if (env != nullptr && env[0] != '\0') {
    const int n = std::atoi(env);
    if (n >= 1) {
      return n;
    }
  }
#endif
  return DefaultWorkerCount();
}

/// RAII decrement of the in-flight counter (every exit path of a job).
struct InFlightGuard {
  std::atomic<int> &counter;
  ~InFlightGuard() { counter.fetch_sub(1); }
};

}  // namespace

ChunkStreamer::ChunkStreamer(World &world, const Atlas &atlas, int worker_count) : world_(world), atlas_(atlas) {
  // At least one worker is required: chunk loading only happens on workers.
  const int count = std::max(1, (worker_count > 0) ? worker_count : ConfiguredWorkerCount());
  workers_.reserve(static_cast<size_t>(std::max(0, count)));
  for (int i = 0; i < count; ++i) {
    workers_.emplace_back([this] { WorkerLoop(); });
  }
  LOG_INFO("Voxel") << "Chunk streamer: " << workers_.size() << " worker thread(s) "
                    << "(generation + meshing run off the render thread)";
}

ChunkStreamer::~ChunkStreamer() {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    stop_ = true;
  }
  queue_cv_.notify_all();
  result_space_cv_.notify_all();
  for (std::thread &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void ChunkStreamer::QueueMesh(int cx, int cz, bool jump_queue) {
  const int64_t  key = Key(cx, cz);
  const uint64_t id  = next_id_++;
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    cancelled_.erase(key);  // (re)queued: a stale cancel must not eat this job
    Job job{cx, cz, key, id};
    if (jump_queue) {
      queue_.push_front(job);
    } else {
      queue_.push_back(job);
    }
  }
  chunks_[key] = ChunkState{cx, cz, Status::Queued, id, false, false};
  in_flight_.fetch_add(1);
  queue_cv_.notify_one();
}

void ChunkStreamer::Cancel(int64_t key) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  cancelled_.insert(key);
}

std::vector<std::pair<int, int>> ChunkStreamer::SetCenter(int center_cx, int center_cz, int load_radius,
                                                          int keep_radius) {
  const int load_r = std::max(0, load_radius);
  const int keep_r = std::max(load_r + 2, keep_radius);  // neighbours must stay available

  // Chunks that stay tracked: the keep ring, plus the load ring in distance
  // order so the jobs we push below are already sorted nearest-first.
  std::unordered_set<int64_t> keep;
  keep.reserve(static_cast<size_t>((2 * keep_r + 1) * (2 * keep_r + 1)));
  std::vector<std::pair<int, int>> load_ring;
  load_ring.reserve(static_cast<size_t>((2 * load_r + 1) * (2 * load_r + 1)));
  std::unordered_set<int64_t> load;
  load.reserve(load_ring.capacity());
  for (int dz = -keep_r; dz <= keep_r; ++dz) {
    for (int dx = -keep_r; dx <= keep_r; ++dx) {
      keep.insert(Key(center_cx + dx, center_cz + dz));
    }
  }
  for (int r = 0; r <= load_r; ++r) {
    for (int dz = -r; dz <= r; ++dz) {
      for (int dx = -r; dx <= r; ++dx) {
        if (std::max(std::abs(dx), std::abs(dz)) != r) {
          continue;
        }
        const int cx = center_cx + dx;
        const int cz = center_cz + dz;
        load_ring.emplace_back(cx, cz);
        load.insert(Key(cx, cz));
      }
    }
  }

  std::vector<std::pair<int, int>> dropped;
  for (auto it = chunks_.begin(); it != chunks_.end();) {
    if (keep.count(it->first) == 0) {
      // Fell outside the keep ring: cancel any queued job (so workers skip the
      // work) and hand the chunk back for teardown.
      Cancel(it->first);
      dropped.emplace_back(it->second.cx, it->second.cz);
      it = chunks_.erase(it);
      continue;
    }
    // Inside the keep ring: resident chunks are simply no longer "wanted" —
    // they keep their data, entities and meshes, so they stay on screen.
    it->second.resident = load.count(it->first) == 0;
    ++it;
  }

  // Queue the chunks that are inside the load radius and not tracked yet. A
  // resident chunk that comes back into the load radius is already meshed (and
  // its voxel data never changed), so it is reused as-is, with no job at all.
  for (const auto &[cx, cz] : load_ring) {
    if (chunks_.count(Key(cx, cz)) != 0) {
      continue;
    }
    QueueMesh(cx, cz, /*jump_queue=*/false);
  }
  return dropped;
}

void ChunkStreamer::RequestRemesh(int cx, int cz) {
  if (chunks_.count(Key(cx, cz)) == 0) {
    return;  // not tracked (outside the keep ring): it will be built fresh when it is
  }
  QueueMesh(cx, cz, /*jump_queue=*/true);
}

void ChunkStreamer::RequestRemeshWithNeighbours(int cx, int cz) {
  RequestRemesh(cx, cz);
  RequestRemesh(cx - 1, cz);
  RequestRemesh(cx + 1, cz);
  RequestRemesh(cx, cz - 1);
  RequestRemesh(cx, cz + 1);
}

void ChunkStreamer::Drain(std::vector<ChunkMesh> &out, int max_count) {
  std::lock_guard<std::mutex> lock(result_mutex_);
  while (max_count > 0 && !results_.empty()) {
    --max_count;
    ChunkMesh mesh = std::move(results_.front());
    results_.pop_front();
    result_space_cv_.notify_one();

    const auto state = chunks_.find(Key(mesh.cx, mesh.cz));
    if (state == chunks_.end() || state->second.job_id != mesh.job_id) {
      continue;  // chunk left the radius, or a newer request superseded this one
    }
    state->second.status   = Status::Meshed;
    state->second.has_mesh = true;
    out.push_back(std::move(mesh));
  }
}

bool ChunkStreamer::IsReady(int cx, int cz) const {
  const auto it = chunks_.find(Key(cx, cz));
  return it != chunks_.end() && it->second.has_mesh;
}

bool ChunkStreamer::IsResident(int cx, int cz) const {
  const auto it = chunks_.find(Key(cx, cz));
  return it != chunks_.end() && it->second.resident;
}

size_t ChunkStreamer::ResidentCount() const {
  size_t count = 0;
  for (const auto &[key, state] : chunks_) {
    count += state.resident ? 1u : 0u;
  }
  return count;
}

void ChunkStreamer::WorkerLoop() {
  ChunkSnapshot snapshot;  // reused across jobs (no per-job allocation)
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
      if (queue_.empty()) {
        if (stop_) {
          return;
        }
        continue;
      }
      job = queue_.front();
      queue_.pop_front();
      if (cancelled_.erase(job.key) > 0) {
        in_flight_.fetch_sub(1);  // the render thread no longer wants this chunk
        continue;
      }
    }
    InFlightGuard guard{in_flight_};

    // 1) Make sure this chunk AND its four XZ neighbours exist: generating a
    //    neighbour is just as cheap here as elsewhere (pure function of the
    //    seed) and it is what makes border faces cull correctly. Doing it in
    //    the job means no coordination between jobs is needed.
    world_.EnsureChunk(job.cx, job.cz);
    world_.EnsureChunk(job.cx - 1, job.cz);
    world_.EnsureChunk(job.cx + 1, job.cz);
    world_.EnsureChunk(job.cx, job.cz - 1);
    world_.EnsureChunk(job.cx, job.cz + 1);

    // 2) One lock, one memcpy-ish copy: after this the mesher touches no shared
    //    state at all (no lock, no hash lookup per block).
    if (!world_.FillSnapshot(job.cx, job.cz, snapshot)) {
      continue;  // a neighbour was unloaded meanwhile: a later request retries
    }

    // 3) Mesh on this worker thread (pure CPU, no GL, no engine state).
    ChunkMesh mesh;
    mesh.cx     = job.cx;
    mesh.cz     = job.cz;
    mesh.job_id = job.id;
    BuildChunkMesh(snapshot, atlas_, job.cx, job.cz, mesh.vertices, mesh.indices, mesh.water_vertices,
                   mesh.water_indices);

    // 4) Publish for the render thread, bounded so the pool does not build the
    //    whole world ahead of the per-frame upload budget.
    {
      std::unique_lock<std::mutex> lock(result_mutex_);
      result_space_cv_.wait(lock, [this] { return stop_ || static_cast<int>(results_.size()) < kMaxPendingResults; });
      results_.push_back(std::move(mesh));
    }
  }
}

}  // namespace vox
