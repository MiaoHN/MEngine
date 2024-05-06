#include "core/logger.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <array>
#include <string>

namespace MEngine {

std::shared_ptr<spdlog::logger> Logger::Get(const std::string& name) {
  // All loggers must share the same file sink.
  static auto file_sink =
      std::make_shared<spdlog::sinks::basic_file_sink_st>("MEngine.log", true);
  std::shared_ptr<spdlog::logger> logger = spdlog::get(name);
  if (logger != nullptr) {
    return logger;
  }
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
  std::array<spdlog::sink_ptr, 2> sinks{console_sink, file_sink};
  logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
  spdlog::initialize_logger(logger);
  return logger;
}

}  // namespace MEngine