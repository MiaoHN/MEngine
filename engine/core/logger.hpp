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

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace MEngine {

class Logger {
 public:
  /**
   * @brief Get the logger object.
   *
   * @param name The name of the logger.
   * @return std::shared_ptr<spdlog::logger> The logger object.
   */
  static std::shared_ptr<spdlog::logger> Get(const std::string &name);
};

}  // namespace MEngine