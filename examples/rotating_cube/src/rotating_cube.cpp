#include "rotating_cube.hpp"

#include "core/entry_point.hpp"

RotatingCube::RotatingCube() {}

RotatingCube::~RotatingCube() {}

void RotatingCube::Initialize() {}

void RotatingCube::OnUpdate(float dt) {}

Application *CreateApplication() { return new RotatingCube(); }
