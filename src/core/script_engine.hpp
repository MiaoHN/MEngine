/**
 * @file script_engine.hpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-05-05
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <memory>

#include "core/logger.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace MEngine {

class ScriptEngine {
 public:
  ScriptEngine();
  ~ScriptEngine();

  void LoadScript(const std::string& path);

 private:
  lua_State* L_;

  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace MEngine
