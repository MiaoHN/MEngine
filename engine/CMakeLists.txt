set(SOURCE_CORE
  src/core/entry_point.cpp
  src/core/application.cpp
  src/core/logger.cpp
  src/core/script_engine.cpp
  src/core/uuid.cpp
)

set(SOURCE_RENDER
  src/render/gl.cpp
  src/render/shader.cpp
  src/render/texture.cpp
  src/render/renderer.cpp
  src/render/render_pipeline.cpp
  src/render/render_pass.cpp
  src/render/frame_buffer.cpp
)

set(SOURCE_SCENE
  src/scene/scene.cpp
)

add_library(engine
  ${SOURCE_CORE}
  ${SOURCE_RENDER}
  ${SOURCE_SCENE}
  ${PROJECT_SOURCE_DIR}/deps/glad/src/glad.c
)

target_include_directories(engine
  PRIVATE
  ${PROJECT_SOURCE_DIR}/deps/entt/single_include
  ${PROJECT_SOURCE_DIR}/deps/glm
  ${PROJECT_SOURCE_DIR}/deps/glad/include
  ${PROJECT_SOURCE_DIR}/deps/stb
  ${PROJECT_SOURCE_DIR}/deps/lua
  ${PROJECT_SOURCE_DIR}/engine/src
)

target_compile_definitions(engine PUBLIC _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)

target_link_libraries(engine
  glfw
  lua
  imgui
  ImGuizmo
)
