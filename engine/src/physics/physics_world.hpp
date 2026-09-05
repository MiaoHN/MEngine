/**
 * @file physics_world.hpp
 * @brief Wraps a Jolt Physics simulation (allocators, job system, layer setup).
 */

#pragma once

#include <memory>

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "physics/contact_listener.hpp"
#include "physics/jolt_math.hpp"

namespace MEngine {

/// @brief Owns a Jolt Physics simulation: temp allocator, thread-pool job
/// system, object/broad-phase layer mapping, and the PhysicsSystem itself.
class PhysicsWorld {
 public:
  PhysicsWorld();
  ~PhysicsWorld();

  PhysicsWorld(const PhysicsWorld &)            = delete;
  PhysicsWorld &operator=(const PhysicsWorld &) = delete;

  /// @brief Advances the simulation by `delta_time` seconds (clamped).
  void Update(float delta_time);

  [[nodiscard]] JPH::PhysicsSystem       &GetSystem() { return physics_system_; }
  [[nodiscard]] const JPH::PhysicsSystem &GetSystem() const { return physics_system_; }

  [[nodiscard]] JPH::TempAllocator       &GetTempAllocator() { return *temp_allocator_; }
  [[nodiscard]] JPH::JobSystem           &GetJobSystem() { return *job_system_; }

  /// @brief Creates a box rigid body. Static bodies use the non-moving object
  /// layer; dynamic bodies use the moving layer.
  JPH::BodyID CreateBoxBody(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &half_extents,
                            bool is_dynamic, float friction = 0.5f, float restitution = 0.0f);

  /// @brief Creates a sphere rigid body.
  JPH::BodyID CreateSphereBody(const glm::vec3 &position, const glm::quat &rotation, float radius, bool is_dynamic,
                               float friction = 0.5f, float restitution = 0.0f);

  /// @brief Removes a body from the world (no-op for an invalid id).
  void DestroyBody(JPH::BodyID body_id);

  [[nodiscard]] JPH::BodyInterface &GetBodyInterface() { return physics_system_.GetBodyInterface(); }

  void SetGravity(const glm::vec3 &gravity);

  /// @brief Drains the contact add/remove events recorded since the last call.
  std::vector<ContactEvent> DrainContactEvents() { return contact_listener_.Drain(); }

  /// @brief Clears queued events and the tracked pair set (call when starting
  /// a fresh simulation).
  void ResetContacts() { contact_listener_.Reset(); }

 private:
  /// @brief Two object layers are enough for now: static (0) and dynamic (1).
  class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
   public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override { return true; }
  };

  /// @brief Identity mapping between object layers and broad-phase layers.
  class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
   public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
      return JPH::BroadPhaseLayer(static_cast<JPH::BroadPhaseLayer::Type>(inLayer));
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
      return inLayer.GetValue() == 0 ? "NonMoving" : "Moving";
    }
#endif
  };

  /// @brief Objects may collide with every broad-phase layer.
  class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
   public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override { return true; }
  };

  std::unique_ptr<JPH::TempAllocator> temp_allocator_;
  std::unique_ptr<JPH::JobSystem>     job_system_;

  ObjectLayerPairFilterImpl         object_layer_pair_filter_;
  BroadPhaseLayerInterfaceImpl      broad_phase_layer_interface_;
  ObjectVsBroadPhaseLayerFilterImpl object_vs_broad_phase_filter_;

  ContactListener contact_listener_;

  JPH::PhysicsSystem physics_system_;
};

}  // namespace MEngine
