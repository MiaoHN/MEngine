#include "sandbox.hpp"

Sandbox::Sandbox() {}

Sandbox::~Sandbox() {}

void Sandbox::Initialize() {}

void Sandbox::OnUpdate(float dt) {}

Application *CreateApplication() { return new Sandbox(); }
