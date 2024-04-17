/**
 * @file command.hpp
 * @author your name (you@domain.com)
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

  Command(Type type) : type_(type) {}

  ~Command() {}

 private:
  Type type_;
};

}  // namespace MEngine
