#include "core/logger.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace MEngine {

static std::string LogLevelToString(Logger::Level level) {
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
}

static std::string CurrentTimeStr() {
  auto      now   = std::chrono::system_clock::now();
  auto      now_c = std::chrono::system_clock::to_time_t(now);
  struct tm buffer;

  localtime_s(&buffer, &now_c);

  std::ostringstream oss;
  oss << std::put_time(&buffer, "%Y-%m-%d %H:%M:%S");

  return oss.str();
}

Logger::Logger(const std::string &name, Level level) : name_(name), level_(level) {}

Logger::~Logger() {
  // output format: [time] [level] [name] message
  std::ofstream file(kLogFileName.data(), std::ios::app);
  auto          now   = std::chrono::system_clock::now();
  auto          now_c = std::chrono::system_clock::to_time_t(now);
  file << "[" << CurrentTimeStr() << "] ";
  file << "[" << LogLevelToString(level_) << "] ";
  file << "[" << name_ << "] ";
  file << ss_.str() << std::endl;

  std::cout << "[" << CurrentTimeStr() << "] ";
  std::cout << "[" << LogLevelToString(level_) << "] ";
  std::cout << "[" << name_ << "] ";
  std::cout << ss_.str() << std::endl;

  file.close();
}

std::stringstream &Logger::GetStream() { return ss_; }

}  // namespace MEngine