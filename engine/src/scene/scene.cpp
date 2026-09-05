#include "scene/scene.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
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

/// @brief One mesh draw candidate with everything needed by every pass.
/// Model matrices and world AABBs are computed once per frame and shared by
/// the shadow / SSAO / main passes (CPU frustum culling reads the AABB).
struct RenderItem {
  entt::entity handle{entt::null};
  Ref<Mesh>    mesh;
  Ref<Material> material;  // may be null (shadow / SSAO passes ignore it)
  glm::mat4    model{1.0f};
  glm::vec3    world_min{0.0f};
  glm::vec3    world_max{0.0f};
  bool         has_bounds = false;
};

/// @brief Transforms an object-space AABB into world space (8 corners).
void TransformAABB(const glm::mat4 &model, const glm::vec3 &local_min, const glm::vec3 &local_max,
                   glm::vec3 &world_min, glm::vec3 &world_max) {
  world_min = glm::vec3(std::numeric_limits<float>::max());
  world_max = glm::vec3(std::numeric_limits<float>::lowest());
  const float xs[2] = {local_min.x, local_max.x};
  const float ys[2] = {local_min.y, local_max.y};
  const float zs[2] = {local_min.z, local_max.z};
  for (const float x : xs) {
    for (const float y : ys) {
      for (const float z : zs) {
        const glm::vec3 corner = glm::vec3(model * glm::vec4(x, y, z, 1.0f));
        world_min              = glm::min(world_min, corner);
        world_max              = glm::max(world_max, corner);
      }
    }
  }
}

/// @brief Six frustum planes extracted from a clip matrix (Gribb-Hartmann).
/// A point is inside when dot(plane.xyz, p) + plane.w >= 0.
struct Frustum {
  glm::vec4 planes[6];
};

Frustum ExtractFrustum(const glm::mat4 &proj_view) {
  const glm::mat4 t = glm::transpose(proj_view);
  Frustum         f;
  f.planes[0] = t[3] + t[0];  // left
  f.planes[1] = t[3] - t[0];  // right
  f.planes[2] = t[3] + t[1];  // bottom
  f.planes[3] = t[3] - t[1];  // top
  f.planes[4] = t[3] + t[2];  // near
  f.planes[5] = t[3] - t[2];  // far
  for (auto &plane : f.planes) {
    const float len = glm::length(glm::vec3(plane));
    if (len > 1e-8f) {
      plane /= len;
    }
  }
  return f;
}


/// @brief Verification hook: when MENGINE_NO_BATCH is set, the main pass
/// draws every entity separately (batch size 1). Rendering output must be
/// pixel-identical to the batched path — used by unattended pixel tests.
bool BatchDisabledByEnv() {
  static const bool disabled = std::getenv("MENGINE_NO_BATCH") != nullptr;
  return disabled;
}

/// @brief Total order over material content (pointer-free), used to make
/// equal-content materials adjacent for batching.
bool MaterialLessForBatching(const Ref<Material> &a, const Ref<Material> &b) {
  if (a.get() == b.get()) {
    return false;
  }
  const auto ptr_cmp = [](const Ref<Shader> &x, const Ref<Shader> &y) { return x.get() < y.get(); };
  if (a->GetShader() != b->GetShader()) {
    return ptr_cmp(a->GetShader(), b->GetShader());
  }
  const auto tex_cmp = [](const Ref<Texture> &x, const Ref<Texture> &y) { return x.get() < y.get(); };
  if (a->GetAlbedoMap() != b->GetAlbedoMap()) return tex_cmp(a->GetAlbedoMap(), b->GetAlbedoMap());
  if (a->GetNormalMap() != b->GetNormalMap()) return tex_cmp(a->GetNormalMap(), b->GetNormalMap());
  if (a->GetMetallicRoughnessMap() != b->GetMetallicRoughnessMap()) {
    return tex_cmp(a->GetMetallicRoughnessMap(), b->GetMetallicRoughnessMap());
  }
  if (a->GetAOMap() != b->GetAOMap()) return tex_cmp(a->GetAOMap(), b->GetAOMap());
  const glm::vec4 ca = a->GetBaseColorFactor();
  const glm::vec4 cb = b->GetBaseColorFactor();
  if (std::memcmp(&ca, &cb, sizeof(ca)) != 0) {
    if (ca.r != cb.r) return ca.r < cb.r;
    if (ca.g != cb.g) return ca.g < cb.g;
    if (ca.b != cb.b) return ca.b < cb.b;
    return ca.a < cb.a;
  }
  if (a->GetMetallicFactor() != b->GetMetallicFactor()) return a->GetMetallicFactor() < b->GetMetallicFactor();
  if (a->GetRoughnessFactor() != b->GetRoughnessFactor()) return a->GetRoughnessFactor() < b->GetRoughnessFactor();
  return a->GetSpecularFactor() < b->GetSpecularFactor();
}

/// @brief True when two materials are interchangeable for draw batching: same
/// shader, same texture slots and identical factors. (Materials are often
/// per-entity objects with equal content — batching must merge those.)
bool SameMaterialForBatching(const Ref<Material> &a, const Ref<Material> &b) {
  if (a.get() == b.get()) {
    return true;
  }
  if (!a || !b) {
    return false;
  }
  return !MaterialLessForBatching(a, b) && !MaterialLessForBatching(b, a);
}

/// @brief True when the AABB is at least partially inside the frustum.
bool AABBInsideFrustum(const Frustum &f, const glm::vec3 &world_min, const glm::vec3 &world_max) {
  for (const auto &plane : f.planes) {
    const glm::vec3 n(plane);
    // Corner closest to the plane along +n: if it is fully outside, the whole
    // box is outside this plane (and thus outside the frustum).
    const glm::vec3 p_pos(n.x >= 0.0f ? world_max.x : world_min.x,
                          n.y >= 0.0f ? world_max.y : world_min.y,
                          n.z >= 0.0f ? world_max.z : world_min.z);
    if (glm::dot(n, p_pos) + plane.w < 0.0f) {
      return false;
    }
  }
  return true;
}

}  // namespace}  // namespace

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

entt::entity Scene::Raycast(const glm::vec3 &origin, const glm::vec3 &direction, float max_distance,
                            float *out_distance) const {
  JPH::BodyID body;
  float       distance = 0.0f;
  if (!physics_world_->Raycast(origin, direction, max_distance, body, distance)) return entt::null;

  const auto it = body_id_to_entity_.find(body.GetIndexAndSequenceNumber());
  if (it == body_id_to_entity_.end()) return entt::null;
  if (out_distance) *out_distance = distance;
  return it->second;
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
  using clock = std::chrono::steady_clock;
  const auto time_ms = [](clock::time_point start) {
    return static_cast<float>(std::chrono::duration<double, std::milli>(clock::now() - start).count());
  };

  renderer_->ResetFrameStats();
  for (float &t : pass_times_ms_) {
    t = 0.0f;
  }

  // Collect every renderable mesh entity once per frame: one model-matrix
  // computation, one world AABB (from the mesh's cached object-space AABB)
  // shared by the shadow/SSAO/main passes below.
  std::vector<RenderItem> items;
  items.reserve(16);
  glm::vec3 scene_min(std::numeric_limits<float>::max());
  glm::vec3 scene_max(std::numeric_limits<float>::lowest());
  for (auto &entity : GetAllEntitiesWith<MeshComponent>()) {
    if (!IsRenderable(entity)) {
      continue;
    }
    auto &component = entity.GetComponent<MeshComponent>();
    RenderItem item;
    item.handle   = entity.GetHandle();
    item.mesh     = component.mesh;
    item.material = component.material;
    item.model    = entity.HasComponent<Transform>() ? entity.GetComponent<Transform>().GetTransform()
                                                     : glm::mat4(1.0f);
    glm::vec3 local_min;
    glm::vec3 local_max;
    if (component.mesh && component.mesh->GetLocalBounds(local_min, local_max)) {
      TransformAABB(item.model, local_min, local_max, item.world_min, item.world_max);
      item.has_bounds = true;
      scene_min       = glm::min(scene_min, item.world_min);
      scene_max       = glm::max(scene_max, item.world_max);
    }
    items.push_back(std::move(item));
  }

  if (items.empty()) {
    return;
  }

  // TAA jitters the camera projection by a sub-pixel each frame; the main pass
  // and skybox use the jittered projection, then the TAA resolve blends it with
  // the history buffer before post-processing.
  const bool      taa_enabled = renderer_->IsTAAEnabled();
  const glm::mat4 render_proj = taa_enabled ? renderer_->GetJitteredProjection(proj) : proj;
  const glm::mat4 proj_view   = render_proj * view;

  // Camera frustum used to cull the per-camera passes (main + SSAO). Shadow
  // passes intentionally render everything: the light sees a different set.
  const Frustum frustum = ExtractFrustum(proj_view);

  // Directional shadow mapping: fit the orthographic shadow volume to the
  // scene's world-space bounds so every object casts a shadow instead of being
  // clipped by a fixed-size frustum.
  const auto &light = renderer_->GetLight();
  glm::mat4   light_view_proj;
  if (scene_max.x > scene_min.x && scene_max.y > scene_min.y && scene_max.z > scene_min.z) {
    const glm::vec3 scene_center = (scene_min + scene_max) * 0.5f;
    const float     scene_radius = std::max(glm::length(scene_max - scene_min) * 0.5f, 0.5f);
    light_view_proj              = light.GetLightSpaceMatrix(scene_center, scene_radius);
  } else {
    light_view_proj = light.GetLightSpaceMatrix(glm::vec3(0.0f), 2.0f);
  }

  // Directional shadow pass: all renderable meshes, batched per mesh so
  // identical meshes go out as one instanced draw.
  renderer_->BeginShadowPass(light_view_proj);
  {
    const auto t_start = clock::now();
    std::vector<const RenderItem *> ordered;
    ordered.reserve(items.size());
    for (const auto &item : items) {
      ordered.push_back(&item);
    }
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const RenderItem *a, const RenderItem *b) { return a->mesh.get() < b->mesh.get(); });
    std::vector<glm::mat4> batch;
    for (size_t i = 0; i < ordered.size();) {
      size_t j = i + 1;
      while (j < ordered.size() && ordered[j]->mesh == ordered[i]->mesh) {
        ++j;
      }
      batch.clear();
      batch.reserve(j - i);
      for (size_t k = i; k < j; ++k) {
        batch.push_back(ordered[k]->model);
      }
      renderer_->DrawMeshShadowInstanced(ordered[i]->mesh, batch.data(), static_cast<int>(batch.size()),
                                         light_view_proj);
      i = j;
    }
    renderer_->EndShadowPass();
    pass_times_ms_[0] = time_ms(t_start);
  }

  // Omnidirectional (point light) shadow passes: one cube map per
  // shadow-casting point light, rendered face by face.
  {
    const auto        t_start = clock::now();
    const auto &point_lights = renderer_->GetPointLights();
    for (size_t i = 0; i < point_lights.size(); ++i) {
      const int shadow_index = renderer_->GetPointShadowIndex(static_cast<int>(i));
      if (shadow_index < 0) {
        continue;
      }

      const auto transforms = point_lights[i].GetShadowMatrices();
      std::vector<const RenderItem *> ordered;
      ordered.reserve(items.size());
      for (const auto &item : items) {
        ordered.push_back(&item);
      }
      std::stable_sort(ordered.begin(), ordered.end(),
                       [](const RenderItem *a, const RenderItem *b) { return a->mesh.get() < b->mesh.get(); });
      renderer_->BeginPointShadowPass(shadow_index, point_lights[i].position, point_lights[i].radius);
      for (int face = 0; face < 6; ++face) {
        renderer_->BindPointShadowFace(shadow_index, face, transforms[face]);
        std::vector<glm::mat4> batch;
        for (size_t k = 0; k < ordered.size();) {
          size_t j = k + 1;
          while (j < ordered.size() && ordered[j]->mesh == ordered[k]->mesh) {
            ++j;
          }
          batch.clear();
          batch.reserve(j - k);
          for (size_t m = k; m < j; ++m) {
            batch.push_back(ordered[m]->model);
          }
          renderer_->DrawMeshPointShadowInstanced(ordered[k]->mesh, batch.data(), static_cast<int>(batch.size()));
          k = j;
        }
      }
      renderer_->EndPointShadowPass(shadow_index);
    }
    pass_times_ms_[1] = time_ms(t_start);
  }

  // SSAO geometry pass (view-space position + normal), then the AO passes.
  // Uses the same camera-frustum culling as the main pass.
  const auto t_ssao_start = clock::now();
  if (renderer_->IsSSAOEnabled()) {
    renderer_->BeginSSAOPass(proj, view);
    std::vector<const RenderItem *> visible;
    visible.reserve(items.size());
    for (const auto &item : items) {
      if (item.has_bounds && !AABBInsideFrustum(frustum, item.world_min, item.world_max)) {
        continue;
      }
      visible.push_back(&item);
    }
    std::stable_sort(visible.begin(), visible.end(),
                     [](const RenderItem *a, const RenderItem *b) { return a->mesh.get() < b->mesh.get(); });
    std::vector<glm::mat4> batch;
    for (size_t k = 0; k < visible.size();) {
      size_t j = k + 1;
      while (j < visible.size() && visible[j]->mesh == visible[k]->mesh) {
        ++j;
      }
      batch.clear();
      batch.reserve(j - k);
      for (size_t m = k; m < j; ++m) {
        batch.push_back(visible[m]->model);
      }
      renderer_->DrawMeshSSAOInstanced(visible[k]->mesh, batch.data(), static_cast<int>(batch.size()));
      k = j;
    }
    renderer_->EndSSAOPass();
    renderer_->GenerateSSAO(proj, view);
  }
  pass_times_ms_[2] = time_ms(t_ssao_start);

  // Main pass into the HDR scene framebuffer: camera-frustum culling plus the
  // mesh+material requirement.
  uint64_t visible_main = 0;
  uint64_t culled_main  = 0;
  {
    renderer_->BeginScene();
    const auto t_start = clock::now();
    std::vector<const RenderItem *> visible;
    visible.reserve(items.size());
    for (const auto &item : items) {
      if (!item.material || !item.material->GetShader()) {
        ++culled_main;  // counted as not drawn by the main pass
        continue;
      }
      if (item.has_bounds && !AABBInsideFrustum(frustum, item.world_min, item.world_max)) {
        ++culled_main;
        continue;
      }
      ++visible_main;
      visible.push_back(&item);
    }
    // Batch by (mesh, material content): same mesh + interchangeable material
    // drawn as one instanced draw; material uniforms are uploaded once per
    // batch. Pointer sorting keeps equal-content materials adjacent.
    std::stable_sort(visible.begin(), visible.end(), [](const RenderItem *a, const RenderItem *b) {
      if (a->mesh.get() != b->mesh.get()) {
        return a->mesh.get() < b->mesh.get();
      }
      return MaterialLessForBatching(a->material, b->material);
    });
    std::vector<glm::mat4> batch;
    const bool no_batch = BatchDisabledByEnv();
    for (size_t k = 0; k < visible.size();) {
      size_t j = k + 1;
      while (!no_batch && j < visible.size() && visible[j]->mesh == visible[k]->mesh &&
             SameMaterialForBatching(visible[j]->material, visible[k]->material)) {
        ++j;
      }
      batch.clear();
      batch.reserve(j - k);
      for (size_t m = k; m < j; ++m) {
        batch.push_back(visible[m]->model);
      }
      renderer_->DrawMeshInstanced(visible[k]->mesh, visible[k]->material, batch.data(),
                                   static_cast<int>(batch.size()), proj_view, camera_pos, light_view_proj);
      k = j;
    }
    pass_times_ms_[3] = time_ms(t_start);
  }

  // Skybox background (drawn after the meshes with depth test LEQUAL).
  {
    const auto t_start = clock::now();
    renderer_->RenderSkybox(view, render_proj);
    renderer_->EndScene();
    pass_times_ms_[4] = time_ms(t_start);
  }

  // TAA resolve blends the jittered frame with the history buffer.
  renderer_->ResolveTAA();

  // Bloom + tone mapping to the target framebuffer.
  {
    const auto t_start = clock::now();
    renderer_->PostProcess(view, proj, target_fbo, target_width, target_height);
    pass_times_ms_[5] = time_ms(t_start);
  }

  renderer_->RecordCulledEntities(culled_main);

  // Periodic unattended-friendly summary of the frame's render work. One row
  // every 120 rendered frames so even very short headless runs emit stats.
  if (++stats_log_frames_ >= 120) {
    stats_log_frames_ = 0;
    const auto &stats = renderer_->GetFrameStats();
    LOG_INFO("RenderStats") << "drawcalls=" << stats.draw_calls << " instanced=" << stats.instanced_draws
                            << " triangles=" << stats.triangles << " culled=" << stats.culled_entities
                            << " visible_main=" << visible_main
                            << " [ms] shadow=" << pass_times_ms_[0] << " point=" << pass_times_ms_[1]
                            << " ssao=" << pass_times_ms_[2] << " main=" << pass_times_ms_[3]
                            << " post=" << pass_times_ms_[5];
  }
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

void Scene::GetLastPassTimes(float out_times[6]) const {
  for (int i = 0; i < 6; ++i) {
    out_times[i] = pass_times_ms_[i];
  }
}

const RenderStats &Scene::GetRenderStats() const { return renderer_->GetFrameStats(); }

void Scene::SetRenderMode(RenderMode mode) { renderer_->SetRenderMode(mode); }

RenderMode Scene::GetRenderMode() const { return renderer_->GetRenderMode(); }

}  // namespace MEngine
