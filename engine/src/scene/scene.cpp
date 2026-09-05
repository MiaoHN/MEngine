#include "scene/scene.hpp"

#include <algorithm>
#include <limits>

#include <glm/gtx/euler_angles.hpp>

#include "core/input.hpp"
#include "render/renderer.hpp"
#include "scene/component.hpp"

namespace MEngine {

namespace {

/// @brief True when the entity should take part in lighting/shadow passes: it
/// has a valid mesh and is not an editor-only helper (e.g. the grid).
bool IsRenderable(Entity &entity) {
  if (entity.HasComponent<Tag>() && entity.GetComponent<Tag>().editor_only) {
    return false;
  }
  return entity.HasComponent<MeshComponent>() && entity.GetComponent<MeshComponent>().mesh != nullptr;
}

/// @brief Computes the world-space AABB of every renderable mesh entity.
bool ComputeSceneBounds(std::vector<Entity> &entities, glm::vec3 &out_min, glm::vec3 &out_max) {
  bool     initialized = false;
  glm::vec3 min_v(std::numeric_limits<float>::max());
  glm::vec3 max_v(std::numeric_limits<float>::lowest());

  for (auto &entity : entities) {
    if (!entity.HasComponent<MeshComponent>() || !entity.GetComponent<MeshComponent>().mesh) continue;
    if (entity.HasComponent<Tag>() && entity.GetComponent<Tag>().editor_only) continue;

    const glm::mat4 model =
        entity.HasComponent<Transform>() ? entity.GetComponent<Transform>().GetTransform() : glm::mat4(1.0f);
    for (const auto &vertex : entity.GetComponent<MeshComponent>().mesh->GetVertices()) {
      const glm::vec3 world = glm::vec3(model * glm::vec4(vertex.position, 1.0f));
      min_v                  = glm::min(min_v, world);
      max_v                  = glm::max(max_v, world);
      initialized            = true;
    }
  }

  if (!initialized) return false;
  out_min = min_v;
  out_max = max_v;
  return true;
}

}  // namespace

Scene::Scene() {
  default_camera_info_                = CreateRef<Camera>();
  default_camera_info_->projection_type = ProjectionType::Orthographic;
  default_camera_info_->ortho_size      = 5.0f;
  default_camera_info_->near_plane      = -100.0f;
  default_camera_info_->far_plane       = 100.0f;
  default_camera_info_->position        = glm::vec3(0.0f);

  renderer_ = CreateRef<Renderer>();

  physics_world_ = CreateRef<PhysicsWorld>();

  script_engine_ = CreateRef<ScriptEngine>(this);
}

Scene::~Scene() {}

Entity Scene::FindEntityByName(const std::string &name) {
  for (auto &entity : entities_) {
    if (entity.HasComponent<Tag>() && entity.GetComponent<Tag>().tag == name) {
      return entity;
    }
  }
  return Entity();
}

void Scene::RefreshEntityBody(entt::entity handle) {
  if (!simulating_) return;

  // Invalid handle (entity destroyed / reused): drop its body if any.
  if (handle == entt::null || !registry_.valid(handle)) {
    const auto it = body_ids_.find(handle);
    if (it != body_ids_.end()) {
      physics_world_->DestroyBody(it->second);
      body_id_to_entity_.erase(it->second.GetIndexAndSequenceNumber());
      body_ids_.erase(it);
    }
    return;
  }

  const auto *group      = registry_.try_get<ColliderGroupComponent>(handle);
  const bool  has_primary = registry_.all_of<ColliderComponent>(handle);
  const bool  has_group   = group != nullptr && !group->Empty();
  const bool  eligible = registry_.all_of<Transform>(handle) && registry_.all_of<RigidBodyComponent>(handle) &&
                         (has_primary || has_group);
  const auto it  = body_ids_.find(handle);
  const bool has = it != body_ids_.end();

  if (eligible && !has) {
    const auto &rigid_body = registry_.get<RigidBodyComponent>(handle);
    const auto &transform  = registry_.get<Transform>(handle);
    const bool  is_dynamic = rigid_body.type == RigidBodyComponent::Type::Dynamic;
    const glm::quat rotation = glm::quat(glm::radians(transform.rotation));

    // Translate component shapes into physics shape descriptions.
    const auto primary_to_desc = [](const ColliderComponent &c) {
      ColliderShapeDesc d;
      d.kind         = static_cast<ColliderShapeDesc::Kind>(static_cast<int>(c.shape));
      d.half_extents = c.box_half_extents;
      d.radius       = c.ShapeRadius();
      d.half_height  = (c.shape == ColliderComponent::Shape::Capsule) ? c.capsule_half_height : c.cylinder_half_height;
      d.offset       = c.offset;
      return d;
    };
    const auto data_to_desc = [](const ColliderShapeData &s) {
      ColliderShapeDesc d;
      d.kind         = static_cast<ColliderShapeDesc::Kind>(static_cast<int>(s.shape));
      d.half_extents = s.box_half_extents;
      d.radius       = (s.shape == ColliderShapeData::Shape::Box) ? 0.0f : 0.5f;  // overwritten below
      switch (s.shape) {
        case ColliderShapeData::Shape::Sphere: d.radius = s.sphere_radius; break;
        case ColliderShapeData::Shape::Capsule:
          d.radius = s.capsule_radius;
          d.half_height = s.capsule_half_height;
          break;
        case ColliderShapeData::Shape::Cylinder:
          d.radius = s.cylinder_radius;
          d.half_height = s.cylinder_half_height;
          break;
        case ColliderShapeData::Shape::Box:
        default: break;
      }
      d.offset = s.offset;
      return d;
    };

    JPH::BodyID body_id;
    if (has_primary && !has_group) {
      // Legacy single-collider path (keeps body at translation + primary offset).
      const auto &collider  = registry_.get<ColliderComponent>(handle);
      const glm::vec3 position = transform.translation + collider.offset;
      switch (collider.shape) {
        case ColliderComponent::Shape::Sphere:
          body_id = physics_world_->CreateSphereBody(position, rotation, collider.sphere_radius, is_dynamic,
                                                     rigid_body.friction, rigid_body.restitution,
                                                     rigid_body.is_sensor, rigid_body.continuous_collision);
          break;
        case ColliderComponent::Shape::Capsule:
          body_id = physics_world_->CreateCapsuleBody(position, rotation, collider.capsule_half_height,
                                                      collider.capsule_radius, is_dynamic, rigid_body.friction,
                                                      rigid_body.restitution, rigid_body.is_sensor,
                                                      rigid_body.continuous_collision);
          break;
        case ColliderComponent::Shape::Cylinder:
          body_id = physics_world_->CreateCylinderBody(position, rotation, collider.cylinder_half_height,
                                                       collider.cylinder_radius, is_dynamic, rigid_body.friction,
                                                       rigid_body.restitution, rigid_body.is_sensor,
                                                       rigid_body.continuous_collision);
          break;
        case ColliderComponent::Shape::Box:
        default:
          body_id = physics_world_->CreateBoxBody(position, rotation, collider.box_half_extents, is_dynamic,
                                                  rigid_body.friction, rigid_body.restitution, rigid_body.is_sensor,
                                                  rigid_body.continuous_collision);
          break;
      }
    } else {
      // Compound body: ColliderGroupComponent (and the primary collider if any)
      // are merged; each shape offset is a local offset, body centre == entity.
      std::vector<ColliderShapeDesc> shapes;
      if (has_primary) {
        shapes.push_back(primary_to_desc(registry_.get<ColliderComponent>(handle)));
      }
      for (const auto &s : group->shapes) {
        shapes.push_back(data_to_desc(s));
      }
      body_id = physics_world_->CreateBody(transform.translation, rotation, is_dynamic, rigid_body.friction,
                                           rigid_body.restitution, shapes, rigid_body.is_sensor,
                                           rigid_body.continuous_collision);
    }
    if (body_id.IsInvalid()) {
      LOG_WARN("Scene") << "Failed to create physics body for entity " << entt::to_integral(handle);
      return;
    }
    body_ids_[handle]                              = body_id;
    body_id_to_entity_[body_id.GetIndexAndSequenceNumber()] = handle;
  } else if (!eligible && has) {
    physics_world_->DestroyBody(it->second);
    body_id_to_entity_.erase(it->second.GetIndexAndSequenceNumber());
    body_ids_.erase(it);
  }
}

bool Scene::HasPhysicsBody(entt::entity handle) {
  return simulating_ && body_ids_.count(handle) != 0;
}

glm::vec3 Scene::GetBodyVelocity(entt::entity handle) {
  if (!simulating_) return glm::vec3(0.0f);
  const auto it = body_ids_.find(handle);
  if (it == body_ids_.end()) return glm::vec3(0.0f);
  return ToGlm(physics_world_->GetBodyInterface().GetLinearVelocity(it->second));
}

void Scene::SetBodyVelocity(entt::entity handle, const glm::vec3 &velocity) {
  if (!simulating_) return;
  const auto it = body_ids_.find(handle);
  if (it == body_ids_.end()) return;
  // Only dynamic bodies may be moved directly.
  const auto *rigid = registry_.try_get<RigidBodyComponent>(handle);
  if (!rigid || rigid->type == RigidBodyComponent::Type::Static) return;

  auto &body_interface = physics_world_->GetBodyInterface();
  body_interface.ActivateBody(it->second);
  body_interface.SetLinearVelocity(it->second, ToJolt(velocity));
}

void Scene::ApplyBodyImpulse(entt::entity handle, const glm::vec3 &impulse) {
  if (!simulating_) return;
  const auto it = body_ids_.find(handle);
  if (it == body_ids_.end()) return;
  const auto *rigid = registry_.try_get<RigidBodyComponent>(handle);
  if (!rigid || rigid->type == RigidBodyComponent::Type::Static) return;

  auto &body_interface = physics_world_->GetBodyInterface();
  body_interface.ActivateBody(it->second);
  body_interface.AddImpulse(it->second, ToJolt(impulse));
}

void Scene::StartSimulation() {
  if (simulating_) {
    LOG_WARN("Scene") << "StartSimulation called while already simulating";
    return;
  }

  body_ids_.clear();
  body_id_to_entity_.clear();
  // Drop any queued events / tracked pairs left over from a previous run.
  physics_world_->ResetContacts();
  sim_accumulator_ = 0.0f;
  simulating_      = true;

  // Remember the authoring scene so StopSimulation can restore it exactly.
  CapturePlaySnapshot();

  // Build bodies for every entity that is currently eligible.
  SyncSimulationBodies();
  LOG_INFO("Scene") << "Physics simulation started with " << body_ids_.size() << " bodies";
}

void Scene::StepSimulation(float delta_time) {
  if (!simulating_) return;

  // Pick up bodies for entities spawned / reconfigured at runtime.
  SyncSimulationBodies();

  // Fixed-step loop: physics, collision dispatch and script OnFixedUpdate all
  // advance at kFixedTimeStep, so a frame with N accumulated steps runs
  // [step physics -> write back -> collisions -> OnFixedUpdate] N times. This
  // keeps OnFixedUpdate and OnCollisionEnter/Exit consistent with the physics.
  constexpr float kMaxFrameDt = 0.25f;  // clamp a hitch so we never spiral
  constexpr int   kMaxSteps   = 5;
  sim_accumulator_ += std::min(delta_time, kMaxFrameDt);

  int steps = 0;
  while (sim_accumulator_ >= kFixedTimeStep && steps < kMaxSteps) {
    sim_accumulator_ -= kFixedTimeStep;

    physics_world_->Update(kFixedTimeStep);
    WriteBackTransforms();
    DispatchContactEvents();
    script_engine_->FixedStepUpdate(kFixedTimeStep);
    ++steps;
  }
  if (steps >= kMaxSteps) {
    sim_accumulator_ = 0.0f;  // drop the excess after a hitch
  }
}

void Scene::WriteBackTransforms() {
  auto &body_interface = physics_world_->GetBodyInterface();
  for (auto &[handle, body_id] : body_ids_) {
    auto *transform = registry_.try_get<Transform>(handle);
    if (transform == nullptr) continue;

    // Legacy single collider: body is placed at translation + collider.offset,
    // so undo the offset when writing back. Compound bodies (ColliderGroup)
    // keep shape offsets inside the body, so their centre is the translation.
    glm::vec3 offset{0.0f};
    const auto *group = registry_.try_get<ColliderGroupComponent>(handle);
    if (!(group && !group->Empty())) {
      if (auto *collider = registry_.try_get<ColliderComponent>(handle)) {
        offset = collider->offset;
      }
    }

    transform->translation = ToGlm(body_interface.GetPosition(body_id)) - offset;
    transform->rotation    = glm::degrees(glm::eulerAngles(ToGlm(body_interface.GetRotation(body_id))));
  }
}

void Scene::DispatchContactEvents() {
  auto &body_interface = physics_world_->GetBodyInterface();

  for (const auto &event : physics_world_->DrainContactEvents()) {
    const auto it_a = body_id_to_entity_.find(event.body_a.GetIndexAndSequenceNumber());
    const auto it_b = body_id_to_entity_.find(event.body_b.GetIndexAndSequenceNumber());
    if (it_a == body_id_to_entity_.end() || it_b == body_id_to_entity_.end()) continue;

    const entt::entity a = it_a->second;
    const entt::entity b = it_b->second;

    if (!event.added) {
      // Exit events carry no contact data (Jolt only gives us the pair).
      ScriptCollisionInfo empty;
      script_engine_->DispatchCollision(a, b, empty, false);
      script_engine_->DispatchCollision(b, a, empty, false);
      continue;
    }

    // Orient the data for each receiving script *before* dispatching, so a
    // handler that mutates/destroys the scene can't invalidate the reads.
    const glm::vec3 va = ToGlm(body_interface.GetLinearVelocity(event.body_a));
    const glm::vec3 vb = ToGlm(body_interface.GetLinearVelocity(event.body_b));
    const glm::vec3 n  = ToGlm(event.normal);  // manifold normal: body_a -> body_b

    // For entity `a` (self = a, other = b): normal from other->self = -n;
    // relative velocity = velocity(other) - velocity(self) = vb - va.
    ScriptCollisionInfo for_a;
    for_a.point             = ToGlm(event.point);
    for_a.normal            = -n;
    for_a.relative_velocity = vb - va;
    for_a.penetration       = event.penetration;
    script_engine_->DispatchCollision(a, b, for_a, true);

    // For entity `b` (self = b, other = a): normal = +n, rel = va - vb.
    ScriptCollisionInfo for_b;
    for_b.point             = for_a.point;
    for_b.normal            = n;
    for_b.relative_velocity = va - vb;
    for_b.penetration       = for_a.penetration;
    script_engine_->DispatchCollision(b, a, for_b, true);
  }
}

void Scene::SyncSimulationBodies() {
  if (!simulating_) return;

  // Drop bodies whose entities were destroyed at runtime.
  for (auto it = body_ids_.begin(); it != body_ids_.end();) {
    if (it->first == entt::null || !registry_.valid(it->first)) {
      physics_world_->DestroyBody(it->second);
      body_id_to_entity_.erase(it->second.GetIndexAndSequenceNumber());
      it = body_ids_.erase(it);
    } else {
      ++it;
    }
  }

  // Refresh every entity that has (or had) physics-relevant components.
  for (auto &entity : entities_) {
    const entt::entity handle = entity.GetHandle();
    if (registry_.all_of<RigidBodyComponent>(handle) || registry_.all_of<ColliderComponent>(handle) ||
        registry_.all_of<ColliderGroupComponent>(handle)) {
      RefreshEntityBody(handle);
    }
  }
}

void Scene::StopSimulation() {
  if (!simulating_) return;

  for (auto &[handle, body_id] : body_ids_) {
    physics_world_->DestroyBody(body_id);
  }
  body_ids_.clear();
  body_id_to_entity_.clear();
  sim_accumulator_ = 0.0f;
  simulating_      = false;

  // Return to the authoring scene captured at StartSimulation: restores any
  // transforms/materials scripts changed, removes entities they spawned and
  // re-creates entities they destroyed (editor-only helpers are preserved).
  RestorePlaySnapshot();

  LOG_INFO("Scene") << "Physics simulation stopped and play snapshot restored";
}

void Scene::UpdateCameraControllers(float delta_time, const glm::vec2 &mouse_delta, bool look_active) {
  for (auto &entity : GetAllEntitiesWith<CameraController, CameraComponent>()) {
    auto   &controller = entity.GetComponent<CameraController>();
    auto   &component  = entity.GetComponent<CameraComponent>();
    Camera &camera     = component.camera;

    // Look around (yaw on Y, pitch on X); clamp pitch so the camera never
    // flips upside down.
    if (look_active) {
      camera.rotation.x -= mouse_delta.y * controller.look_sensitivity;
      camera.rotation.y -= mouse_delta.x * controller.look_sensitivity;
      camera.rotation.x  = glm::clamp(camera.rotation.x, -89.0f, 89.0f);
    }

    // Free-fly movement along the camera's local axes.
    float forward = 0.0f;
    float right   = 0.0f;
    float up      = 0.0f;
    if (Input::IsKeyPressed(GLFW_KEY_W)) forward += 1.0f;
    if (Input::IsKeyPressed(GLFW_KEY_S)) forward -= 1.0f;
    if (Input::IsKeyPressed(GLFW_KEY_D)) right += 1.0f;
    if (Input::IsKeyPressed(GLFW_KEY_A)) right -= 1.0f;
    if (Input::IsKeyPressed(GLFW_KEY_E)) up += 1.0f;
    if (Input::IsKeyPressed(GLFW_KEY_Q)) up -= 1.0f;

    if (forward != 0.0f || right != 0.0f || up != 0.0f) {
      const glm::vec3 f = camera.GetForward();
      const glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0.0f, 1.0f, 0.0f)));
      const glm::vec3 u = glm::vec3(0.0f, 1.0f, 0.0f);
      camera.position += (f * forward + r * right + u * up) * controller.move_speed * delta_time;
    }
  }
}

void Scene::OnUpdateEditor(const Camera &camera) { Render(camera); }

void Scene::OnUpdateSimulation(float dt, const Camera &camera) {
  (void)dt;
  // TODO: Update scene status
  Render(camera);
}

void Scene::OnUpdateRuntime(float dt, int vw, int vh) {
  (void)dt;
  // TODO: Implement
  bool has_primary_camera = false;
  for (auto &entity : GetAllEntitiesWith<CameraComponent>()) {
    auto &component = entity.GetComponent<CameraComponent>();
    if (component.primary) {
      has_primary_camera = true;
      component.camera.SetAspect(static_cast<float>(vw) / static_cast<float>(vh));
      Render(component.camera);
    }
  }
  if (!has_primary_camera) {
    Render(*GetDefaultCameraInfo());
  }
}

void Scene::Render(const Camera &camera) {
  const glm::mat4 proj_view = camera.GetProjectionView();
  for (auto &entity : GetAllEntitiesWith<Sprite2D>()) {
    auto &sprite = entity.GetComponent<Sprite2D>();
    renderer_->RenderSprite(sprite, proj_view);
  }
  for (auto &entity : GetAllEntitiesWith<AnimatedSprite2D>()) {
    auto &sprite = entity.GetComponent<AnimatedSprite2D>();
    renderer_->RenderSprite(sprite, proj_view);
  }
}

void Scene::RenderFromPrimaryCamera(unsigned int target_fbo, int target_width, int target_height) {
  const float aspect = (target_width > 0 && target_height > 0)
                           ? static_cast<float>(target_width) / static_cast<float>(target_height)
                           : 16.0f / 9.0f;

  const Camera *active = nullptr;
  for (auto &entity : GetAllEntitiesWith<CameraComponent>()) {
    auto &component = entity.GetComponent<CameraComponent>();
    if (component.primary) {
      component.camera.SetAspect(aspect);
      active = &component.camera;
      break;
    }
  }

  if (!active) {
    default_camera_info_->SetAspect(aspect);
    active = default_camera_info_.get();
  }

  RenderMeshes(active->GetViewMatrix(), active->GetProjectionMatrix(), active->GetPosition(), target_fbo, target_width,
               target_height);
}

bool Scene::HasPrimaryCamera() {
  for (auto &entity : GetAllEntitiesWith<CameraComponent>()) {
    if (entity.GetComponent<CameraComponent>().primary) {
      return true;
    }
  }
  return false;
}

void Scene::RenderMeshes(const glm::mat4 &view, const glm::mat4 &proj, const glm::vec3 &camera_pos,
                         unsigned int target_fbo, int target_width, int target_height) {
  auto entities = GetAllEntitiesWith<MeshComponent>();
  if (entities.empty()) {
    return;
  }

  // TAA jitters the camera projection by a sub-pixel each frame; the main pass
  // and skybox use the jittered projection, then the TAA resolve blends it with
  // the history buffer before post-processing.
  const bool       taa_enabled   = renderer_->IsTAAEnabled();
  const glm::mat4  render_proj   = taa_enabled ? renderer_->GetJitteredProjection(proj) : proj;
  const glm::mat4  proj_view     = render_proj * view;

  // Directional shadow mapping: fit the orthographic shadow volume to the
  // scene's world-space bounds so every object casts a shadow instead of being
  // clipped by a fixed-size frustum.
  const auto &light = renderer_->GetLight();
  glm::mat4   light_view_proj;
  {
    glm::vec3 scene_min;
    glm::vec3 scene_max;
    if (ComputeSceneBounds(entities, scene_min, scene_max)) {
      const glm::vec3 scene_center = (scene_min + scene_max) * 0.5f;
      const float     scene_radius = std::max(glm::length(scene_max - scene_min) * 0.5f, 0.5f);
      light_view_proj              = light.GetLightSpaceMatrix(scene_center, scene_radius);
    } else {
      light_view_proj = light.GetLightSpaceMatrix(glm::vec3(0.0f), 2.0f);
    }
  }

  renderer_->BeginShadowPass(light_view_proj);
  for (auto &entity : entities) {
    if (!IsRenderable(entity)) {
      continue;
    }
    auto &component = entity.GetComponent<MeshComponent>();
    const glm::mat4 model =
        entity.HasComponent<Transform>() ? entity.GetComponent<Transform>().GetTransform() : glm::mat4(1.0f);
    renderer_->DrawMeshShadow(component.mesh, model, light_view_proj);
  }
  renderer_->EndShadowPass();

  // Omnidirectional (point light) shadow passes: one cube map per
  // shadow-casting point light, rendered face by face.
  {
    const auto &point_lights = renderer_->GetPointLights();
    for (size_t i = 0; i < point_lights.size(); ++i) {
      const int shadow_index = renderer_->GetPointShadowIndex(static_cast<int>(i));
      if (shadow_index < 0) {
        continue;
      }

      const auto transforms = point_lights[i].GetShadowMatrices();
      renderer_->BeginPointShadowPass(shadow_index, point_lights[i].position, point_lights[i].radius);
      for (int face = 0; face < 6; ++face) {
        renderer_->BindPointShadowFace(shadow_index, face, transforms[face]);
        for (auto &entity : entities) {
          if (!IsRenderable(entity)) {
            continue;
          }
          auto &component = entity.GetComponent<MeshComponent>();
          const glm::mat4 model =
              entity.HasComponent<Transform>() ? entity.GetComponent<Transform>().GetTransform() : glm::mat4(1.0f);
          renderer_->DrawMeshPointShadow(component.mesh, model);
        }
      }
      renderer_->EndPointShadowPass(shadow_index);
    }
  }

  // SSAO geometry pass (view-space position + normal), then the AO passes.
  if (renderer_->IsSSAOEnabled()) {
    renderer_->BeginSSAOPass(proj, view);
    for (auto &entity : entities) {
      auto &component = entity.GetComponent<MeshComponent>();
      if (!component.mesh) {
        continue;
      }
      const glm::mat4 model =
          entity.HasComponent<Transform>() ? entity.GetComponent<Transform>().GetTransform() : glm::mat4(1.0f);
      renderer_->DrawMeshSSAO(component.mesh, model);
    }
    renderer_->EndSSAOPass();
    renderer_->GenerateSSAO(proj, view);
  }

  // Main pass into the HDR scene framebuffer.
  renderer_->BeginScene();
  for (auto &entity : entities) {
    auto &component = entity.GetComponent<MeshComponent>();
    if (!component.mesh || !component.material) {
      continue;
    }

    const glm::mat4 model =
        entity.HasComponent<Transform>() ? entity.GetComponent<Transform>().GetTransform() : glm::mat4(1.0f);

    renderer_->DrawMesh(component.mesh, component.material, model, proj_view, camera_pos, light_view_proj);
  }

  // Skybox background (drawn after the meshes with depth test LEQUAL).
  renderer_->RenderSkybox(view, render_proj);
  renderer_->EndScene();

  // TAA resolve blends the jittered frame with the history buffer.
  renderer_->ResolveTAA();

  // Bloom + tone mapping to the target framebuffer.
  renderer_->PostProcess(view, proj, target_fbo, target_width, target_height);
}

void Scene::AddPointLight(const PointLight &light) { renderer_->AddPointLight(light); }

void Scene::ClearPointLights() { renderer_->ClearPointLights(); }

void Scene::AddSpotLight(const SpotLight &light) { renderer_->AddSpotLight(light); }

void Scene::ClearSpotLights() { renderer_->ClearSpotLights(); }

const DirectionalLight &Scene::GetLight() const { return renderer_->GetLight(); }

DirectionalLight &Scene::GetLight() { return renderer_->GetLight(); }

void Scene::SetLight(const DirectionalLight &light) { renderer_->SetLight(light); }

void Scene::SetExposure(float exposure) { renderer_->SetExposure(exposure); }

void Scene::SetBloomStrength(float strength) { renderer_->SetBloomStrength(strength); }

void Scene::SetBloomThreshold(float threshold) { renderer_->SetBloomThreshold(threshold); }

void Scene::SetShadowPcfRadius(float radius) { renderer_->SetShadowPcfRadius(radius); }

void Scene::SetIblIntensity(float intensity) { renderer_->SetIblIntensity(intensity); }

void Scene::SetGodRaysStrength(float strength) { renderer_->SetGodRaysStrength(strength); }

void Scene::SetSSAOEnabled(bool enabled) { renderer_->SetSSAOEnabled(enabled); }

void Scene::SetTAAEnabled(bool enabled) { renderer_->SetTAAEnabled(enabled); }

void Scene::SetBloomEnabled(bool enabled) { renderer_->SetBloomEnabled(enabled); }

bool Scene::IsSSAOEnabled() const { return renderer_->IsSSAOEnabled(); }

bool Scene::IsTAAEnabled() const { return renderer_->IsTAAEnabled(); }

bool Scene::IsBloomEnabled() const { return renderer_->IsBloomEnabled(); }

float Scene::GetExposure() const { return renderer_->GetExposure(); }

float Scene::GetBloomStrength() const { return renderer_->GetBloomStrength(); }

float Scene::GetBloomThreshold() const { return renderer_->GetBloomThreshold(); }

float Scene::GetShadowPcfRadius() const { return renderer_->GetShadowPcfRadius(); }

float Scene::GetIblIntensity() const { return renderer_->GetIblIntensity(); }

float Scene::GetGodRaysStrength() const { return renderer_->GetGodRaysStrength(); }

void Scene::SetRenderMode(RenderMode mode) { renderer_->SetRenderMode(mode); }

RenderMode Scene::GetRenderMode() const { return renderer_->GetRenderMode(); }

}  // namespace MEngine
