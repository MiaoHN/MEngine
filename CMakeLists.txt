cmake_minimum_required(VERSION 3.20)

project(MEngine VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(MENGINE_ENABLE_VULKAN "Enable Vulkan backend" ON)

if(MENGINE_ENABLE_VULKAN)
  find_package(Vulkan QUIET)
  if(Vulkan_FOUND)
    message(STATUS "Vulkan SDK found. Vulkan backend enabled.")
  else()
    message(STATUS "Vulkan SDK not found. Vulkan backend will compile in fallback mode.")
  endif()
endif()

add_subdirectory(deps/glfw)

# setup lua
set(LUA_BUILD_AS_DLL OFF CACHE BOOL "Build Lua as a DLL")
file(GLOB LUA_SOURCE_FILES deps/lua/*.c)
list(REMOVE_ITEM LUA_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/deps/lua/onelua.c")
add_library(lua STATIC ${LUA_SOURCE_FILES})
target_include_directories(lua PUBLIC deps/lua)

# setup imgui
file(GLOB IMGUI_SOURCE_FILES deps/imgui/*.cpp
  deps/imgui/backends/imgui_impl_glfw.cpp
  deps/imgui/backends/imgui_impl_opengl3.cpp
)
if(Vulkan_FOUND)
  list(APPEND IMGUI_SOURCE_FILES deps/imgui/backends/imgui_impl_vulkan.cpp)
endif()
add_library(imgui STATIC ${IMGUI_SOURCE_FILES})
target_include_directories(imgui PUBLIC deps/imgui)
target_include_directories(imgui PUBLIC deps/imgui/backends)
target_link_libraries(imgui PRIVATE glfw)
if(Vulkan_FOUND)
  target_link_libraries(imgui PUBLIC Vulkan::Vulkan)
endif()

# setup ImGuiZmo
file(GLOB IMGUIZMO_SOURCE_FILES deps/ImGuizmo/*.cpp)
add_library(ImGuizmo STATIC ${IMGUIZMO_SOURCE_FILES})
target_include_directories(ImGuizmo PUBLIC
  deps/ImGuizmo
  deps/imgui
)

add_subdirectory(engine)

add_subdirectory(editor)

add_subdirectory(examples)