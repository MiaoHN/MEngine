#pragma once

// Core aggregator header. NOTE: `Ref`/`CreateRef` live in core/base.hpp; the
// imgui / GLFW / glm includes below are still here so existing TUs keep
// compiling, but new code should include exactly what it needs (see P1 in
// docs/DEV-PLAN.md — this coupling is being pruned).

#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// clang-format off
// #include <ImGuizmo.h>
// clang-format on

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef ERROR
#undef ERROR
#endif

#include "core/base.hpp"
#include "core/logger.hpp"