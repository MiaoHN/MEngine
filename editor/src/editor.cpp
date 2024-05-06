#include "editor.hpp"

#include "core/logger.hpp"

Editor::Editor() {}

Editor::~Editor() {}

void Editor::Initialize() {}

Application* CreateApplication() { return new Editor(); }