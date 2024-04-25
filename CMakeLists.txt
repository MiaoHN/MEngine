cmake_minimum_required(VERSION 3.20)

project(MEngine VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_subdirectory(deps/glfw)

set(SOURCE_CORE
  src/core/main.cpp
  src/core/application.cpp
  src/core/logger.cpp
  src/core/task_dispatcher.cpp
  src/core/task_handler.cpp
)

set(SOURCE_RENDER
  src/render/gl.cpp
  src/render/shader.cpp
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
)

target_link_libraries(${PROJECT_NAME}
    glfw
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/res
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
)