#include "physics/physics_world.hpp"

#include <algorithm>
#include <thread>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

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
  // Jolt's default allocator must be registered exactly once per process.
  static bool allocator_registered = false;
  if (!allocator_registered) {
    JPH::RegisterDefaultAllocator();
    allocator_registered = true;
  }

  temp_allocator_ = std::make_unique<JPH::TempAllocatorMalloc>();

  const int num_threads = std::max(0, static_cast<int>(std::thread::hardware_concurrency()) - 1);
  job_system_ = std::make_unique<JPH::JobSystemThreadPool>(kMaxPhysicsJobs, kMaxPhysicsBarriers, num_threads);

  physics_system_.Init(kMaxBodies, kNumBodyMutexes, kMaxBodyPairs, kMaxContactConstraints,
                       broad_phase_layer_interface_, object_vs_broad_phase_filter_, object_layer_pair_filter_);
  physics_system_.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

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
  JPH::BoxShapeSettings     shape_settings(ToJolt(half_extents));
  JPH::BodyCreationSettings body_settings(&shape_settings, ToJolt(position), ToJolt(rotation),
                                          is_dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                          is_dynamic ? JPH::ObjectLayer(1) : JPH::ObjectLayer(0));
  body_settings.mFriction    = friction;
  body_settings.mRestitution = restitution;
  return physics_system_.GetBodyInterface().CreateBody(body_settings)->GetID();
}

JPH::BodyID PhysicsWorld::CreateSphereBody(const glm::vec3 &position, const glm::quat &rotation, float radius,
                                           bool is_dynamic, float friction, float restitution) {
  JPH::SphereShapeSettings   shape_settings(radius);
  JPH::BodyCreationSettings  body_settings(&shape_settings, ToJolt(position), ToJolt(rotation),
                                           is_dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                                           is_dynamic ? JPH::ObjectLayer(1) : JPH::ObjectLayer(0));
  body_settings.mFriction    = friction;
  body_settings.mRestitution = restitution;
  return physics_system_.GetBodyInterface().CreateBody(body_settings)->GetID();
}

void PhysicsWorld::DestroyBody(JPH::BodyID body_id) {
  if (!body_id.IsInvalid()) {
    physics_system_.GetBodyInterface().DestroyBody(body_id);
  }
}

void PhysicsWorld::SetGravity(const glm::vec3 &gravity) { physics_system_.SetGravity(ToJolt(gravity)); }

}  // namespace MEngine
