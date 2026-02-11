#pragma once

#include "mengine.hpp"

using namespace MEngine;

class RotatingCube : public Application {
 public:
  RotatingCube();
  ~RotatingCube() override;

  void Initialize() override;
  void OnUpdate(float dt) override;

 private:
  Ref<Scene> scene_;
  Ref<Camera2D> camera_;
  
  Entity cube_entity_;
  float rotation_speed_ = 45.0f;  // degrees per second
};