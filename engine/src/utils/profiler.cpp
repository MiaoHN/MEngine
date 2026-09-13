#include "profiler.h"

#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace MEngine {
namespace detail {

uint64_t CurrentThreadId() { return std::hash<std::thread::id>{}(std::this_thread::get_id()); }

uint64_t CurrentProcessId() {
#if defined(_WIN32)
  return static_cast<uint64_t>(::GetCurrentProcessId());
#else
  return static_cast<uint64_t>(::getpid());
#endif
}

}  // namespace detail

namespace {

#if PROFILER_ENABLED

/// @brief Lazily opens `profile_results.json` on first event and closes the
/// JSON array when the process exits. Guarded by a global mutex.
class TraceWriter {
 public:
  ~TraceWriter() {
    if (out_.is_open()) {
      out_ << "]}";
      out_.close();
    }
  }

  void Write(const ProfileResult &result) {
    std::lock_guard<std::mutex> lock(mutex_);
    EnsureOpen();
    if (!out_.is_open()) return;

    std::stringstream ss;
    ss << std::setprecision(3) << std::fixed;
    ss << (first_event_ ? "" : ",");
    ss << "{\"cat\":\"function\",";
    ss << "\"dur\":" << result.GetDuration() << ",";
    ss << "\"name\":\"" << result.GetName() << "\",";
    ss << "\"ph\":\"X\",";
    ss << "\"pid\":" << result.GetProcessId() << ",";
    ss << "\"tid\":" << result.GetThreadId() << ",";
    ss << "\"ts\":" << result.GetStartTime();
    ss << "}";
    first_event_ = false;

    out_ << ss.str();
    out_.flush();
  }

 private:
  void EnsureOpen() {
    if (opened_) return;
    opened_ = true;
    out_.open("profile_results.json", std::ios::out | std::ios::trunc);
    if (!out_.is_open()) {
      std::cerr << "Profiler: failed to open profile_results.json" << std::endl;
      return;
    }
    out_ << "{\"otherData\": {},\"traceEvents\":[";
  }

  std::mutex    mutex_;
  std::ofstream out_;
  bool          opened_     = false;
  bool          first_event_ = true;
};

TraceWriter g_trace_writer;

#endif  // PROFILER_ENABLED

}  // namespace

void ProfilerCollector::Collect(const ProfileResult &profile_result) {
#if PROFILER_ENABLED
  g_trace_writer.Write(profile_result);
#else
  (void)profile_result;  // profiling disabled: record nothing, write no file
#endif
}

}  // namespace MEngine