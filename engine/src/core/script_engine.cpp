#include "core/script_engine.hpp"

namespace MEngine {

ScriptEngine::ScriptEngine() { L_ = luaL_newstate(); }

ScriptEngine::~ScriptEngine() { lua_close(L_); }

void ScriptEngine::LoadScript(const std::string &path) {
  LOG_INFO("ScriptEngine") << "Loading script: " << path;

  if (luaL_loadfile(L_, path.c_str()) || lua_pcall(L_, 0, 0, 0)) {
    LOG_ERROR("ScriptEngine") << "Error loading script: " << lua_tostring(L_, -1);
    lua_close(L_);
  }

  luaL_openlibs(L_);

  lua_getglobal(L_, "init");
  if (!lua_isfunction(L_, -1)) {
    LOG_ERROR("ScriptEngine") << "Script does not contain init function";
    lua_close(L_);
  }

  if (lua_pcall(L_, 0, 0, 0)) {
    LOG_ERROR("ScriptEngine") << "Error running init function: " << lua_tostring(L_, -1);
    lua_close(L_);
  }
}

}  // namespace MEngine
