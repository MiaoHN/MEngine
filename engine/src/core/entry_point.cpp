#include "core/entry_point.hpp"

#include <string>

#include "core/application.hpp"
#include "core/logger.hpp"

using namespace MEngine;

int main(int argc, char const *argv[]) {
  // Parse a `--scene <path>` startup argument (used by standalone play).
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--scene") {
      Application::SetStartupScenePath(argv[i + 1]);
    }
  }

  LOG_INFO("MAIN") << "Application started";

  Application *app = CreateApplication();

  app->Initialize();

  app->Run();

  delete app;

  LOG_INFO("MAIN") << "Application terminated";

  return 0;
}
