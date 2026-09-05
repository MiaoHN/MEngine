#include "physics/physics_world.hpp"

#include <algorithm>
#include <thread>

#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/RegisterTypes.h>

#include "core/logger.hpp"

namespace MEngine {

namespace {
constexpr int kMaxBodies             = 4096;
constexpr int kNumBodyMutexes        = 0;  // 0 = let Jolt pick a default
constexpr int kMaxBodyPairs          = 8192;
constexpr int kMaxContactConstraints = 2048;
constexpr int kMaxPhysicsJobs        = 2048;
constexpr int kMaxPhysicsBarriers    = 8;
}  // namespace

PhysicsWorld::PhysicsWorld() {
  // Jolt's default allocator, object factory and type registry must all be
  // initialized exactly once per process. The type registry also installs the
  // shape-vs-shape collision functions — without it the narrow phase crashes
  // on the first contact.
  static bool jolt_registered = false;
  if (!jolt_registered) {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    jolt_registered = true;
  }

  temp_allocator_ = std::make_unique<JPH::TempAllocatorMalloc>();

  const int num_threads = std::max(0, static_cast<int>(std::thread::hardware_concurrency()) - 1);
  job_system_ = std::make_unique<JPH::JobSystemThreadPool>(kMaxPhysicsJobs, kMaxPhysicsBarriers, num_threads);

  physics_system_.Init(kMaxBodies, kNumBodyMutexes, kMaxBodyPairs, kMaxContactConstraints,
                       broad_phase_layer_interface_, object_vs_broad_phase_filter_, object_layer_pair_filter_);
  physics_system_.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
  physics_system_.SetContactListener(&contact_listener_);

  LOG_INFO("Physics") << "Jolt Physics world initialized";
}

PhysicsWorld::~PhysicsWorld() {
  // Destroy the job system before the allocator it may depend on.
  job_system_.reset();
  temp_allocator_.reset();
}

void PhysicsWorld::Update(float delta_time) {
  // Clamp the step so a paused/blocked frame doesn't tunnel bodies.
  const float clamped_dt = std::clamp(delta_time, 0.0f, 0.1f);
  physics_system_.Update(clamped_dt, 1, temp_allocator_.get(), job_system_.get());
}

JPH::BodyID PhysicsWorld::CreateBoxBody(const glm::vec3 &position, const glm::quat &rotation,
                                        const glm::vec3 &half_extents, bool is_dynamic, float friction,
                                        float restitution) {
  // NOTE: pass a heap-allocated concrete shape. BodyCreationSettings takes a
  // ref-counted ownership of it and releases it on destruction — passing the
  // address of a stack-allocated ShapeSettings would make Release() delete
  // the stack object (crash).
  JPH::BodyCreationSettings body_settings(new JPH::BoxShape(ToJolt(half_extents)), ToJolt(position), ToJolt(rotation),
                                          is_dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                          is_dynamic ? JPH::ObjectLayer(1) : JPH::ObjectLayer(0));
  body_settings.mFriction    = friction;
  body_settings.mRestitution = restitution;
  return physics_system_.GetBodyInterface().CreateAndAddBody(
      body_settings, is_dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

JPH::BodyID PhysicsWorld::CreateSphereBody(const glm::vec3 &position, const glm::quat &rotation, float radius,
                                           bool is_dynamic, float friction, float restitution) {
  JPH::BodyCreationSettings body_settings(new JPH::SphereShape(radius), ToJolt(position), ToJolt(rotation),
                                          is_dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                          is_dynamic ? JPH::ObjectLayer(1) : JPH::ObjectLayer(0));
  body_settings.mFriction    = friction;
  body_settings.mRestitution = restitution;
  return physics_system_.GetBodyInterface().CreateAndAddBody(
      body_settings, is_dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

JPH::BodyID PhysicsWorld::CreateCapsuleBody(const glm::vec3 &position, const glm::quat &rotation, float half_height,
                                            float radius, bool is_dynamic, float friction, float restitution) {
  // Jolt CapsuleShape is oriented along the local Y axis; match that convention.
  JPH::BodyCreationSettings body_settings(new JPH::CapsuleShape(half_height, radius), ToJolt(position),
                                          ToJolt(rotation),
                                          is_dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                          is_dynamic ? JPH::ObjectLayer(1) : JPH::ObjectLayer(0));
  body_settings.mFriction    = friction;
  body_settings.mRestitution = restitution;
  return physics_system_.GetBodyInterface().CreateAndAddBody(
      body_settings, is_dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

JPH::BodyID PhysicsWorld::CreateCylinderBody(const glm::vec3 &position, const glm::quat &rotation, float half_height,
                                             float radius, bool is_dynamic, float friction, float restitution) {
  // Jolt CylinderShape is oriented along the local Y axis; match that convention.
  JPH::BodyCreationSettings body_settings(new JPH::CylinderShape(half_height, radius), ToJolt(position),
                                          ToJolt(rotation),
                                          is_dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                          is_dynamic ? JPH::ObjectLayer(1) : JPH::ObjectLayer(0));
  body_settings.mFriction    = friction;
  body_settings.mRestitution = restitution;
  return physics_system_.GetBodyInterface().CreateAndAddBody(
      body_settings, is_dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

JPH::BodyID PhysicsWorld::CreateBody(const glm::vec3 &position, const glm::quat &rotation, bool is_dynamic,
                                     float friction, float restitution, const std::vector<ColliderShapeDesc> &shapes) {
  if (shapes.empty()) return {};

  // Build a compound (Jolt merges a single shape into a trivial compound too;
  // each shape's offset is kept as a local offset so body position == entity
  // translation).
  JPH::StaticCompoundShapeSettings settings;
  for (const auto &s : shapes) {
    JPH::Ref<JPH::Shape> sub;
    switch (s.kind) {
      case ColliderShapeDesc::Kind::Box:
        sub = new JPH::BoxShape(ToJolt(s.half_extents));
        break;
      case ColliderShapeDesc::Kind::Sphere:
        sub = new JPH::SphereShape(s.radius);
        break;
      case ColliderShapeDesc::Kind::Capsule:
        sub = new JPH::CapsuleShape(s.half_height, s.radius);
        break;
      case ColliderShapeDesc::Kind::Cylinder:
        sub = new JPH::CylinderShape(s.half_height, s.radius);
        break;
    }
    settings.AddShape(ToJolt(s.offset), JPH::Quat::sIdentity(), sub.GetPtr());
  }

  const JPH::ShapeSettings::ShapeResult result = settings.Create();
  if (result.HasError()) {
    LOG_ERROR("Physics") << "Failed to create compound body: " << result.GetError();
    return {};
  }

  JPH::Ref<JPH::Shape> compound = result.Get();
  JPH::BodyCreationSettings body_settings(compound.GetPtr(), ToJolt(position), ToJolt(rotation),
                                          is_dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                          is_dynamic ? JPH::ObjectLayer(1) : JPH::ObjectLayer(0));
  body_settings.mFriction    = friction;
  body_settings.mRestitution = restitution;
  return physics_system_.GetBodyInterface().CreateAndAddBody(
      body_settings, is_dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

void PhysicsWorld::DestroyBody(JPH::BodyID body_id) {
  if (body_id.IsInvalid()) return;

  auto &body_interface = physics_system_.GetBodyInterface();
  // A body must be removed from the broad phase and deactivated before it can
  // be destroyed: destroying an active/in-broad-phase body trips a Jolt assert
  // in debug and leaves dangling broad-phase pointers in release.
  body_interface.RemoveBody(body_id);
  body_interface.DestroyBody(body_id);
}

void PhysicsWorld::SetGravity(const glm::vec3 &gravity) { physics_system_.SetGravity(ToJolt(gravity)); }

}  // namespace MEngine
