#include "core/script_engine.hpp"

namespace MEngine {

ScriptEngine::ScriptEngine() {
  logger_ = Logger::Get("script_engine");
  L_      = luaL_newstate();
}

ScriptEngine::~ScriptEngine() { lua_close(L_); }

void ScriptEngine::LoadScript(const std::string &path) {
  logger_->info("Loading script: {0}", path);

  if (luaL_loadfile(L_, path.c_str()) || lua_pcall(L_, 0, 0, 0)) {
    logger_->error("Error loading script: {0}", lua_tostring(L_, -1));
    lua_close(L_);
  }

  luaL_openlibs(L_);

  lua_getglobal(L_, "init");
  if (!lua_isfunction(L_, -1)) {
    logger_->error("Script does not contain init function");
    lua_close(L_);
  }

  if (lua_pcall(L_, 0, 0, 0)) {
    logger_->error("Error running init function: {0}", lua_tostring(L_, -1));
    lua_close(L_);
  }
}

}  // namespace MEngine
