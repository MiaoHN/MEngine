# The "voxel" example target: a Minecraft-like tech demo that consumes the
# ENGINE's public API only (no engine source is modified for it). Terrain,
# meshing, player physics, picking etc. live entirely here.
add_executable(voxel
  src/voxel_atlas.cpp
  src/voxel_world.cpp
  src/voxel_app.cpp
)

target_include_directories(voxel
  PRIVATE
  ${PROJECT_SOURCE_DIR}/engine/src
  src
)

target_include_directories(voxel SYSTEM PRIVATE
  ${PROJECT_SOURCE_DIR}/deps/entt/single_include
  ${PROJECT_SOURCE_DIR}/deps/glm
  ${PROJECT_SOURCE_DIR}/deps/glad/include
  ${PROJECT_SOURCE_DIR}/deps/stb
  ${PROJECT_SOURCE_DIR}/deps/glfw/include
)

target_link_libraries(voxel
  engine
  glfw
)

mengine_enable_warnings(voxel)

# Ship a local copy of the shared assets (skybox / shaders / ...) so the app
# can be run straight from its build directory like the other examples.
add_custom_command(TARGET voxel POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${PROJECT_SOURCE_DIR}/assets
  $<TARGET_FILE_DIR:voxel>/assets
)
