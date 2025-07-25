#include "sandbox.hpp"
#include "utils/profiler.h"

Sandbox::Sandbox() {}

Sandbox::~Sandbox() {}

void Sandbox::Initialize() {}

void Sandbox::OnUpdate(float dt) { PROFILER_FUNCTION(); }

Application *CreateApplication() { return new Sandbox(); }
