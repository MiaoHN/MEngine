/**
 * @file command.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-17
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <string>

namespace MEngine {

/**
 * @brief Command class is a utility class for parsing command line arguments.
 *
 */
class Command {
 public:
  enum class Type {
    Logic,
    Render,
  };

  Command(Type type) : type_(type), is_cancelled_(false) {}

  ~Command() {}

  const bool IsCancelled() const { return is_cancelled_; }

 private:
  Type type_;
  bool is_cancelled_;
};

}  // namespace MEngine
