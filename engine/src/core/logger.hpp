/**
 * @file logger.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-16
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <sstream>

namespace MEngine {

constexpr std::string_view kLogFileName = "mengine.log";

class Logger {
 public:
  enum class Level { TRACE, DEBUG, INFO, WARN, ERROR, FATAL };

  Logger(const std::string &name, Level level);
  ~Logger();

  std::stringstream &GetStream();

 private:
  std::string name_;
  Level       level_;

  std::stringstream ss_;
};

}  // namespace MEngine

#define LOG_TRACE(name) MEngine::Logger(name, MEngine::Logger::Level::TRACE).GetStream()
#define LOG_INFO(name)  MEngine::Logger(name, MEngine::Logger::Level::INFO).GetStream()
#define LOG_WARN(name)  MEngine::Logger(name, MEngine::Logger::Level::WARN).GetStream()
#define LOG_ERROR(name) MEngine::Logger(name, MEngine::Logger::Level::ERROR).GetStream()
#define LOG_FATAL(name) MEngine::Logger(name, MEngine::Logger::Level::FATAL).GetStream()

#ifdef NDEBUG
#define LOG_DEBUG(name)
#else
#define LOG_DEBUG(name) MEngine::Logger(name, MEngine::Logger::Level::DEBUG).GetStream()
#endif
