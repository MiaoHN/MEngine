#include "core/logger.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace MEngine {

namespace {

std::mutex    g_log_mutex;
std::ofstream g_log_file;

std::string LogLevelToString(Logger::Level level) {
  switch (level) {
    case Logger::Level::TRACE:
      return "TRACE";
    case Logger::Level::DEBUG:
      return "DEBUG";
    case Logger::Level::INFO:
      return "INFO";
    case Logger::Level::WARN:
      return "WARN";
    case Logger::Level::ERROR:
      return "ERROR";
    case Logger::Level::FATAL:
      return "FATAL";
  }
  return "UNKNOWN";
}

std::string CurrentTimeStr() {
  using namespace std::chrono;

  const auto now   = system_clock::now();
  const auto now_c = system_clock::to_time_t(now);
  const auto ms    = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  struct tm buffer{};

// localtime_s is MSVC-only; use the POSIX localtime_r everywhere else.
#if defined(_WIN32)
  localtime_s(&buffer, &now_c);
#else
  localtime_r(&now_c, &buffer);
#endif

  std::ostringstream oss;
  oss << std::put_time(&buffer, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count();
  return oss.str();
}

// A single stream kept open for the process lifetime. Opened with truncation
// so each run starts with a fresh, empty log file.
std::ofstream &LogFile() {
  if (!g_log_file.is_open()) {
    g_log_file.open(std::string(kLogFileName), std::ios::trunc);
  }
  return g_log_file;
}

}  // namespace

Logger::Logger(const std::string &name, Level level) : name_(name), level_(level) {}

Logger::~Logger() {
  // Format: [time.ms] [level] [name] message
  const std::string line = "[" + CurrentTimeStr() + "] [" + LogLevelToString(level_) + "] [" + name_ + "] " + ss_.str();

  std::lock_guard<std::mutex> lock(g_log_mutex);
  LogFile() << line << std::endl;
  std::cout << line << std::endl;
}

std::stringstream &Logger::GetStream() { return ss_; }

}  // namespace MEngine