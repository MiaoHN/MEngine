/**
 * @file script_engine.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-05-05
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include "core/common.hpp"

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

  void LoadScript(const std::string &path);

 private:
  lua_State *L_;
};

}  // namespace MEngine
