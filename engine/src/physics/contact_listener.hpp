/**
 * @file contact_listener.hpp
 * @brief Jolt contact listener that records enter/exit events for scripting.
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <vector>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/ContactListener.h>

namespace MEngine {

/// @brief A single collision enter (added=true) or exit (added=false) event
/// between two bodies. On enter events `point`, `normal` and `penetration`
/// describe the contact; exit events only carry the two body ids.
struct ContactEvent {
  JPH::BodyID body_a;
  JPH::BodyID body_b;
  bool        added;
  JPH::Vec3   point;       ///< representative world-space contact point (enter)
  JPH::Vec3   normal;      ///< world normal; points from body_a towards body_b (enter)
  float       penetration; ///< penetration depth (enter)
};

/// @brief Records contact add/remove callbacks into a thread-safe queue that
/// the main thread drains once per physics step.
///
/// Jolt invokes `OnContactAdded`/`OnContactRemoved` per contact *point* (and a
/// box resting on the ground can shed and regain individual points every
/// step), which would spam scripts with spurious enter/exit events. This
/// listener therefore tracks the set of body pairs that are currently in
/// contact and only emits a single `added` event when a pair first starts
/// touching and a single `removed` event when it fully separates.
class ContactListener final : public JPH::ContactListener {
 public:
  void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold,
                      JPH::ContactSettings &) override {
    const uint64_t key = MakePairKey(inBody1.GetID(), inBody2.GetID());
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_pairs_.insert(key).second) return;  // already touching

    ContactEvent event;
    event.body_a      = inBody1.GetID();
    event.body_b      = inBody2.GetID();
    event.added       = true;
    event.penetration = inManifold.mPenetrationDepth;
    event.normal      = inManifold.mWorldSpaceNormal;
    // Representative world contact point on the surface of body 1.
    if (inManifold.mRelativeContactPointsOn1.size() > 0) {
      const JPH::RVec3 p = inManifold.GetWorldSpaceContactPointOn1(0);
      event.point        = JPH::Vec3(static_cast<float>(p.GetX()), static_cast<float>(p.GetY()),
                                     static_cast<float>(p.GetZ()));
    } else {
      event.point = JPH::Vec3::sZero();
    }
    events_.push_back(event);
  }

  void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override {
    const uint64_t key = MakePairKey(inSubShapePair.GetBody1ID(), inSubShapePair.GetBody2ID());
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_pairs_.erase(key) == 0) return;  // never tracked as touching

    ContactEvent event;
    event.body_a = inSubShapePair.GetBody1ID();
    event.body_b = inSubShapePair.GetBody2ID();
    event.added  = false;
    event.point  = JPH::Vec3::sZero();
    event.normal = JPH::Vec3::sZero();
    events_.push_back(event);
  }

  /// @brief Moves all pending events out of the listener.
  std::vector<ContactEvent> Drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ContactEvent> out;
    out.swap(events_);
    return out;
  }

  /// @brief Clears the pending events and the tracked pair set. Call before
  /// starting a fresh simulation so stale state from a previous run (whose
  /// bodies have been destroyed) can never leak into the new one.
  void Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
    active_pairs_.clear();
  }

 private:
  /// @brief Canonical (order-independent) key for an unordered body pair.
  static uint64_t MakePairKey(JPH::BodyID a, JPH::BodyID b) {
    const uint64_t ia = a.GetIndexAndSequenceNumber();
    const uint64_t ib = b.GetIndexAndSequenceNumber();
    return ia < ib ? (ia << 32) | ib : (ib << 32) | ia;
  }

  std::mutex                mutex_;
  std::vector<ContactEvent> events_;
  std::unordered_set<uint64_t> active_pairs_;
};

}  // namespace MEngine
