/**
 * @file jolt_math.hpp
 * @brief Conversion helpers between glm and Jolt Physics math types.
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>

namespace MEngine {

[[nodiscard]] inline JPH::Vec3 ToJolt(const glm::vec3 &v) { return JPH::Vec3(v.x, v.y, v.z); }

[[nodiscard]] inline glm::vec3 ToGlm(const JPH::Vec3 &v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }

[[nodiscard]] inline JPH::Quat ToJolt(const glm::quat &q) { return JPH::Quat(q.x, q.y, q.z, q.w); }

[[nodiscard]] inline glm::quat ToGlm(const JPH::Quat &q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

}  // namespace MEngine
