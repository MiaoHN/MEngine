#pragma once

// Lightweight RAII micro-profiler that emits Chrome `chrome://tracing`
// ("traceEvents") JSON. PROFILER_ENABLED must be 1 to actually record; when it
// is 0 the scope macros expand to nothing and the profiler writes no files.
//
// NOTE: this header must stay self-contained (it is pulled in by core code).

#include <chrono>
#include <cstdint>
#include <string>

#define PROFILER_ENABLED 0

#if PROFILER_ENABLED
#define PROFILER_SCOPE(name) MEngine::Profiler profiler##__LINE__(name)
#define PROFILER_FUNCTION()  MEngine::Profiler profiler##__LINE__(__FUNCTION__)
#else
#define PROFILER_SCOPE(name)
#define PROFILER_FUNCTION()
#endif

namespace MEngine {

namespace detail {
/// @brief Stable id of the current thread / process (used for the
/// chrome://tracing "tid" / "pid" columns). Defined in profiler.cpp so the
/// header never needs <thread> or platform headers.
uint64_t CurrentThreadId();
uint64_t CurrentProcessId();
}  // namespace detail

/// @brief One measured scope: name + duration (us) + start time (us) + ids.
class ProfileResult {
 public:
  ProfileResult(std::string name, long long duration, uint64_t tid, uint64_t pid, uint64_t ts)
      : name_(std::move(name)), duration_(duration), tid_(tid), pid_(pid), ts_(ts) {}

  const char *GetName() const { return name_.c_str(); }
  long long   GetDuration() const { return duration_; }
  uint64_t    GetThreadId() const { return tid_; }
  uint64_t    GetProcessId() const { return pid_; }
  uint64_t    GetStartTime() const { return ts_; }

 private:
  std::string name_;       // name of the profile (owned copy)
  long long   duration_;   // duration in microseconds
  uint64_t    tid_;        // thread ID
  uint64_t    pid_;        // process ID
  uint64_t    ts_;         // start time in microseconds (epoch)
};

/// @brief Routes profile results to the trace writer (no-op unless
/// PROFILER_ENABLED). Thread-safe.
class ProfilerCollector {
 public:
  static void Collect(const ProfileResult &profile_result);
};

/// @brief RAII scope timer. On destruction it reports its measured slice.
class Profiler {
 public:
  explicit Profiler(const char *name) : name_(name), start_time_(StartClock::now()) {}
  explicit Profiler(const std::string &name) : name_(name), start_time_(StartClock::now()) {}

  ~Profiler() { ProfilerCollector::Collect(Dump()); }

  ProfileResult Dump() const {
    const auto end      = StartClock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_time_).count();
    const auto start    = std::chrono::duration_cast<std::chrono::microseconds>(start_time_.time_since_epoch()).count();
    return ProfileResult(name_, duration, detail::CurrentThreadId(), detail::CurrentProcessId(), start);
  }

 private:
  using StartClock = std::chrono::high_resolution_clock;

  std::string                     name_;
  StartClock::time_point          start_time_;
};

}  // namespace MEngine