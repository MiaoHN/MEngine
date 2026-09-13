#pragma once

// Core aggregator header.
//
//  - `Ref`/`CreateRef` live in the lean, dependency-free core/base.hpp.
//  - glm + GLFW remain here for backwards compatibility (glfw is a real engine
//    platform dependency used for input/window); UI consumers must include
//    <imgui.h>/<imgui_internal.h> themselves — this header no longer leaks imgui
//    into non-UI engine TUs. New code should include exactly what it needs.

#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef ERROR
#undef ERROR
#endif

#include "core/base.hpp"
#include "core/logger.hpp"