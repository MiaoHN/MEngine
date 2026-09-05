#include "core/entry_point.hpp"

#include "core/application.hpp"
#include "core/logger.hpp"

using namespace MEngine;

int main(int argc, char const *argv[]) {
  LOG_INFO("MAIN") << "Application started";

  Application *app = CreateApplication();

  app->Initialize();

  app->Run();

  delete app;

  LOG_INFO("MAIN") << "Application terminated";

  return 0;
}
