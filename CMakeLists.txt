cmake_minimum_required(VERSION 3.20)

project(MEngine VERSION 0.0.1)

add_subdirectory(deps/glfw)

add_executable(${PROJECT_NAME}
  deps/glad/src/glad.c
  src/main.cpp
  src/application.cpp
  src/logger.cpp
  src/task_dispatcher.cpp
  src/task_handler.cpp
  src/gl.cpp
  src/shader.cpp
  src/renderer.cpp
  )

target_include_directories(${PROJECT_NAME}
    PRIVATE deps/spdlog/include
    src
    deps/glad/include
)

target_link_libraries(${PROJECT_NAME}
    glfw
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/res
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
)