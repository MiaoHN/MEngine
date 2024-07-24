cmake_minimum_required(VERSION 3.20)

project(MEngine VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

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
add_library(imgui STATIC ${IMGUI_SOURCE_FILES})
target_include_directories(imgui PUBLIC deps/imgui)
target_include_directories(imgui PUBLIC deps/imgui/backends)
target_link_libraries(imgui glfw)

# setup ImGuiZmo
file(GLOB IMGUIZMO_SOURCE_FILES deps/ImGuizmo/*.cpp)
add_library(ImGuizmo STATIC ${IMGUIZMO_SOURCE_FILES})
target_include_directories(ImGuizmo PUBLIC
  deps/ImGuizmo
  deps/imgui
)

set(SOURCE_CORE
  engine/core/entry_point.cpp
  engine/core/application.cpp
  engine/core/logger.cpp
  engine/core/script_engine.cpp
  engine/core/uuid.cpp
)

set(SOURCE_RENDER
  engine/render/gl.cpp
  engine/render/shader.cpp
  engine/render/texture.cpp
  engine/render/renderer.cpp
  engine/render/render_pipeline.cpp
  engine/render/render_pass.cpp
  engine/render/frame_buffer.cpp
)

set(SOURCE_SCENE
  engine/scene/scene.cpp
)

add_library(engine
  ${SOURCE_CORE}
  ${SOURCE_RENDER}
  ${SOURCE_SCENE}
  deps/glad/src/glad.c
)

target_include_directories(engine
  PRIVATE
  deps/spdlog/include
  deps/entt/single_include
  deps/glm
  deps/glad/include
  deps/stb
  deps/lua
  engine
)

target_compile_definitions(engine PUBLIC _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)

target_link_libraries(engine
  glfw
  lua
  imgui
  ImGuizmo
)

add_executable(editor
  editor/src/editor.cpp
)

target_include_directories(editor
  PRIVATE
  deps/spdlog/include
  deps/entt/single_include
  deps/glm
  deps/glad/include
  deps/stb
  deps/imgui
  deps/imgui/backends
  deps/ImGuizmo
  engine
  editor/src
)

target_link_libraries(editor
  engine
)

add_custom_command(TARGET editor POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_SOURCE_DIR}/editor/res
  $<TARGET_FILE_DIR:editor>/res
)

add_custom_command(TARGET editor POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_SOURCE_DIR}/editor/res/imgui.ini
  $<TARGET_FILE_DIR:editor>/imgui.ini
)

add_executable(sandbox
  sandbox/src/sandbox.cpp
)

target_include_directories(sandbox
  PRIVATE
  deps/spdlog/include
  deps/entt/single_include
  deps/glm
  deps/glad/include
  deps/stb
  deps/imgui
  deps/imgui/backends
  deps/ImGuizmo
  engine
  sandbox/src
)

target_link_libraries(sandbox
  engine
)

add_custom_command(TARGET sandbox POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_SOURCE_DIR}/sandbox/res
  $<TARGET_FILE_DIR:sandbox>/res
)

# add_custom_command(TARGET sandbox POST_BUILD
#   COMMAND ${CMAKE_COMMAND} -E copy
#   ${CMAKE_SOURCE_DIR}/sandbox/res/imgui.ini
#   $<TARGET_FILE_DIR:sandbox>/imgui.ini
# )