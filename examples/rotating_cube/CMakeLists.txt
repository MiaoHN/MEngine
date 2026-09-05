add_executable(rotating_cube
  src/rotating_cube.cpp
)

target_include_directories(rotating_cube
  PRIVATE
  ${PROJECT_SOURCE_DIR}/deps/entt/single_include
  ${PROJECT_SOURCE_DIR}/deps/glm
  ${PROJECT_SOURCE_DIR}/deps/glad/include
  ${PROJECT_SOURCE_DIR}/deps/stb
  ${PROJECT_SOURCE_DIR}/deps/imgui
  ${PROJECT_SOURCE_DIR}/deps/imgui/backends
  ${PROJECT_SOURCE_DIR}/deps/ImGuizmo
  ${PROJECT_SOURCE_DIR}/engine/src
  src
)

target_link_libraries(rotating_cube
  engine
)
