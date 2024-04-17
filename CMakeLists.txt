cmake_minimum_required(VERSION 3.20)

project(MEngine VERSION 0.0.1)

add_subdirectory(deps/glfw)

add_executable(engine
  deps/glad/src/glad.c
  src/main.cpp
  src/application.cpp
  src/logger.cpp
  src/task_dispatcher.cpp
  )

target_include_directories(engine
    PRIVATE deps/spdlog/include
    src
    deps/glad/include
)

target_link_libraries(engine
    glfw
)