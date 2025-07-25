#include "profiler.h"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace MEngine {

static ProfilerCollector g_profiler_collector;

ProfilerCollector::ProfilerCollector() {
  global_profiler_ = std::make_unique<Profiler>("GlobalProfiler");

  output_file_.open("profile_results.json", std::ios::out | std::ios::trunc);
  if (!output_file_.is_open()) {
    std::cerr << "Failed to open profile results file." << std::endl;
  }
  output_file_ << "{\"otherData\": {},\"traceEvents\":[{}";
  output_file_.flush();
}

ProfilerCollector::~ProfilerCollector() {
  if (global_profiler_) {
    global_profiler_.reset();
  }

  output_file_ << "]}";

  output_file_.close();
}

void ProfilerCollector::Collect(const ProfileResult &profile_result) {
  std::lock_guard<std::mutex> lock(g_profiler_collector.mutex_);

  std::stringstream ss;

  ss << std::setprecision(3) << std::fixed;
  ss << ",{";
  ss << "\"cat\":\"function\",";
  ss << "\"dur\":" << profile_result.GetDuration() << ",";
  ss << "\"name\":\"" << profile_result.GetName() << "\",";
  ss << "\"ph\":\"X\",";
  ss << "\"pid\": 0,";
  ss << "\"tid\":" << profile_result.GetThreadId() << ",";
  ss << "\"ts\":" << profile_result.GetStartTime();
  ss << "}";

  g_profiler_collector.output_file_ << ss.str();
  g_profiler_collector.output_file_.flush();
}

}  // namespace MEngine