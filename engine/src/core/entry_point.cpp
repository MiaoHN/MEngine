#include "core/entry_point.hpp"

#include <cstdlib>
#include <string>

#include "core/application.hpp"
#include "core/logger.hpp"

using namespace MEngine;

namespace {

/// @brief Parses `--api opengl|vulkan`, `--scene <path>`, `--frames <n>` and
/// `--hidden` from the command line into the shared Application configuration.
void ParseCommandLine(int argc, char const *argv[]) {
  auto value_after = [&](int index, const char *arg) {
    return index + 1 < argc && std::string(argv[index]) == arg ? std::string(argv[index + 1]) : std::string();
  };

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--api") {
      const std::string api = value_after(i++, "--api");
      Application::SetStartupApi(api == "vulkan" ? GraphicsAPI::Vulkan : GraphicsAPI::OpenGL);
    } else if (arg == "--scene") {
      Application::SetStartupScenePath(value_after(i++, "--scene"));
    } else if (arg == "--frames") {
      const std::string frames = value_after(i++, "--frames");
      if (!frames.empty()) {
        Application::SetMaxFrames(std::max(1, std::atoi(frames.c_str())));
      }
    } else if (arg == "--hidden") {
      Application::SetWindowHidden(true);
    }
  }
}

}  // namespace

int main(int argc, char const *argv[]) {
  ParseCommandLine(argc, argv);

  LOG_INFO("MAIN") << "Application started";

  Application *app = CreateApplication();

  app->Initialize();

  app->Run();

  delete app;

  LOG_INFO("MAIN") << "Application terminated";

  return 0;
}
