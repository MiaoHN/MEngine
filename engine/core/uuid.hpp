/**
 * @file uuid.hpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-05-10
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <cinttypes>

namespace MEngine {

class UUID {
 public:
  UUID();
  UUID(uint64_t uuid);
  UUID(const UUID&) = default;

  operator uint64_t() const { return uuid_; }

 private:
  uint64_t uuid_;
};

}  // namespace MEngine

namespace std {
template <typename T>
struct hash;

template <>
struct hash<MEngine::UUID> {
  size_t operator()(const MEngine::UUID& uuid) const { return (uint64_t)uuid; }
};

}  // namespace std