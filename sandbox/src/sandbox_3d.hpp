/**
 * @file sandbox_3d.hpp
 * @brief 3D sandbox: glTF helmet + lights + shadows (see sandbox_2d.hpp for the
 * 2D counterpart).
 *
 * Both sandboxes also act as standalone scene players: `--scene <path>` loads a
 * scene saved by the editor (works for 2D and 3D scenes alike).
 */

#pragma once

#include "core/application.hpp"
#include "core/entry_point.hpp"
#include "core/script_engine.hpp"
#include "render/frame_buffer.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"
#include "scene/camera.hpp"
#include "scene/component.hpp"
#include "scene/entity.hpp"
#include "scene/scene.hpp"

namespace MEngine {

class Sandbox3D : public MEngine::Application {
 public:
  Sandbox3D();
  ~Sandbox3D();

  void Initialize() override;

  void OnUpdate(float dt) override;

 private:
  std::shared_ptr<MEngine::Scene> active_scene_;

  bool running_loaded_scene_ = false;

  MEngine::Entity        model_;
  MEngine::Ref<MEngine::Mesh>     model_mesh_;
  MEngine::Ref<MEngine::Material> model_material_;

  MEngine::Ref<MEngine::Shader> pbr_shader_;

  MEngine::Camera camera_;
  float           rotation_speed_ = 30.0f;
};

}  // namespace MEngine
