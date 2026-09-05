cmake_minimum_required(VERSION 3.20)

project(MEngine VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(MENGINE_ENABLE_VULKAN "Enable Vulkan backend" ON)
option(MENGINE_BUILD_SANDBOX "Build the sandbox example" ON)
option(MENGINE_BUILD_EDITOR "Build the editor" ON)

if(MENGINE_ENABLE_VULKAN)
  find_package(Vulkan QUIET)
  if(Vulkan_FOUND)
    message(STATUS "Vulkan SDK found. Vulkan backend enabled.")
  else()
    message(STATUS "Vulkan SDK not found. Vulkan backend will compile in fallback mode.")
  endif()
endif()

# std::thread / std::mutex are used by the profiler on every platform.
find_package(Threads REQUIRED)

# Shared per-compiler warning flags.
include(cmake/CompilerWarnings.cmake)

add_subdirectory(deps/glfw)

# setup lua
set(LUA_BUILD_AS_DLL OFF CACHE BOOL "Build Lua as a DLL")
file(GLOB LUA_SOURCE_FILES deps/lua/*.c)
list(REMOVE_ITEM LUA_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/deps/lua/onelua.c")
add_library(lua STATIC ${LUA_SOURCE_FILES})
target_include_directories(lua SYSTEM PUBLIC deps/lua)

# setup imgui
file(GLOB IMGUI_SOURCE_FILES deps/imgui/*.cpp
  deps/imgui/backends/imgui_impl_glfw.cpp
  deps/imgui/backends/imgui_impl_opengl3.cpp
)
if(Vulkan_FOUND)
  list(APPEND IMGUI_SOURCE_FILES deps/imgui/backends/imgui_impl_vulkan.cpp)
endif()
add_library(imgui STATIC ${IMGUI_SOURCE_FILES})
# SYSTEM: suppress warnings from third-party headers when they are included
# by our own translation units (GCC/Clang -isystem, MSVC /external:I).
target_include_directories(imgui SYSTEM PUBLIC deps/imgui)
target_include_directories(imgui SYSTEM PUBLIC deps/imgui/backends)
target_link_libraries(imgui PRIVATE glfw)
mengine_quiet_third_party(imgui)
if(Vulkan_FOUND)
  target_link_libraries(imgui PUBLIC Vulkan::Vulkan)
endif()

# setup ImGuiZmo
file(GLOB IMGUIZMO_SOURCE_FILES deps/ImGuizmo/*.cpp)
add_library(ImGuizmo STATIC ${IMGUIZMO_SOURCE_FILES})
target_include_directories(ImGuizmo SYSTEM PUBLIC
  deps/ImGuizmo
  deps/imgui
)
mengine_quiet_third_party(ImGuizmo)

# setup JoltPhysics (CPU-only; options tuned to not clobber MEngine flags)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/jolt/Build)

add_subdirectory(engine)

if(MENGINE_BUILD_SANDBOX)
  add_subdirectory(sandbox)
endif()

if(MENGINE_BUILD_EDITOR)
  add_subdirectory(editor)
endif()