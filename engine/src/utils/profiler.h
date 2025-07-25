#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <vector>

#define PROFILER_ENABLED 0

#if PROFILER_ENABLED
#define PROFILER_SCOPE(name) MEngine::Profiler profiler##__LINE__(name)
#define PROFILER_FUNCTION()  MEngine::Profiler profiler##__LINE__(__FUNCTION__)
#else
#define PROFILER_SCOPE(name)
#define PROFILER_FUNCTION()
#endif

namespace MEngine {

class Profiler;

class ProfileResult {
 public:
  ProfileResult(const char *name, long long duration, uint64_t tid, uint64_t pid, uint64_t ts)
      : name_(name), duration_(duration), tid_(tid), pid_(pid), ts_(ts) {}

  const char *GetName() const { return name_; }
  long long   GetDuration() const { return duration_; }
  uint64_t    GetThreadId() const { return tid_; }
  uint64_t    GetProcessId() const { return pid_; }
  uint64_t    GetStartTime() const { return ts_; }

 private:
  const char *name_;      // @brief name of the profile
  long long   duration_;  // @brief duration in microseconds
  uint64_t    tid_;       // @brief thread ID
  uint64_t    pid_;       // @brief process ID
  uint64_t    ts_;        // @brief start time in microseconds
};

class ProfilerCollector {
 public:
  ProfilerCollector();
  ~ProfilerCollector();

  /**
   * @brief Collects profiling data.
   * @note Only called in Profiler destructor.
   *
   * @param profile_result The profiling result to collect.
   */
  static void Collect(const ProfileResult &profile_result);

 private:
  std::mutex                mutex_;
  std::vector<std::string>  profile_data_;
  std::unique_ptr<Profiler> global_profiler_;
  std::ofstream             output_file_;
};

class Profiler {
 public:
  Profiler(const char *name) : name_(name) { start_time_ = std::chrono::high_resolution_clock::now(); }
  Profiler(const std::string &name) : name_(name.c_str()) { start_time_ = std::chrono::high_resolution_clock::now(); }

  ~Profiler() { ProfilerCollector::Collect(Dump()); }

  // for chrome://tracing
  ProfileResult Dump() const {
    auto     end_time = std::chrono::high_resolution_clock::now();
    auto     duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_).count();
    uint64_t tid      = std::hash<std::thread::id>{}(std::this_thread::get_id());
    uint64_t pid      = std::hash<std::thread::id>{}(std::this_thread::get_id());
    uint64_t ts       = std::chrono::duration_cast<std::chrono::microseconds>(start_time_.time_since_epoch()).count();
    return ProfileResult(name_, duration, tid, pid, ts);
  }

 private:
  const char                                    *name_;
  std::chrono::high_resolution_clock::time_point start_time_;
};

}  // namespace MEngine