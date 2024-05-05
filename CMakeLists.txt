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

set(SOURCE_CORE
  src/core/main.cpp
  src/core/application.cpp
  src/core/logger.cpp
  src/core/task_dispatcher.cpp
  src/core/task_handler.cpp
  src/core/script_engine.cpp
)

set(SOURCE_RENDER
  src/render/gl.cpp
  src/render/shader.cpp
  src/render/texture.cpp
  src/render/renderer.cpp
)

set(SOURCE_SCENE
  src/scene/scene.cpp
)

add_executable(${PROJECT_NAME}
  ${SOURCE_CORE}
  ${SOURCE_RENDER}
  ${SOURCE_SCENE}
  deps/glad/src/glad.c
)

target_include_directories(${PROJECT_NAME}
    PRIVATE deps/spdlog/include
    src
    deps/glad/include
    deps/entt/single_include
    deps/glm
    deps/stb
    deps/lua
)

target_link_libraries(${PROJECT_NAME}
    glfw
    lua
    imgui
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/res
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
)