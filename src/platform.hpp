/**
 * @file platform.hpp
 * @author MiaoHN (582418227@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-04-16
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

namespace MEngine {

/**
 * @brief Utility class for platform-specific operations.
 *
 */
class Platform {
 public:
 private:
  enum class PlatformType { WINDOWS, LINUX, MACOS, IOS, ANDROID, UNKNOWN };

  static PlatformType get_platform_type() {
#if defined(_WIN32)
    return PlatformType::WINDOWS;
#elif defined(__APPLE__)
#include "TargetConditionals.h"
#if TARGET_IPHONE_SIMULATOR
    return PlatformType::IOS;
#elif TARGET_OS_IPHONE
    return PlatformType::IOS;
#elif TARGET_OS_MAC
    return PlatformType::MACOS;
#else
    return PlatformType::UNKNOWN;
#endif
#elif defined(__ANDROID__)
    return PlatformType::ANDROID;
#elif defined(__linux__)
    return PlatformType::LINUX;
#else
    return PlatformType::UNKNOWN;
#endif
  }
};

}  // namespace MEngine
