#include "core/application.hpp"
#include "core/logger.hpp"

using namespace MEngine;

int main(int argc, char const* argv[]) {
  auto logger = Logger::Get("main");

  logger->info("Starting application");

  Application* app = new Application();

  app->Run();

  delete app;

  logger->info("Application terminated successfully");
  return 0;
}
