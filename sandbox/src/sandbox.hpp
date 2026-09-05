/**
 * @file sandbox.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-07-24
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "core/application.hpp"
#include "core/entry_point.hpp"
#include "core/script_engine.hpp"
#include "render/frame_buffer.hpp"
#include "render/material.hpp"
#include "render/mesh.hpp"
#include "render/shader.hpp"
#include "render/texture.hpp"
#include "scene/camera.hpp"
#include "scene/entity.hpp"
#include "scene/scene.hpp"

using namespace MEngine;

class Sandbox : public Application {
 public:
  Sandbox();
  ~Sandbox();

  void Initialize() override;

  void OnUpdate(float dt) override;

 private:
  std::shared_ptr<Scene> active_scene_;

  bool running_loaded_scene_ = false;

  Entity        model_;
  Ref<Mesh>     model_mesh_;
  Ref<Material> model_material_;

  Ref<Shader> pbr_shader_;

  Camera camera_;
  float             rotation_speed_ = 30.0f;
};