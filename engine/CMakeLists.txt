set(SOURCE_CORE
  src/core/entry_point.cpp
  src/core/application.cpp
  src/core/logger.cpp
  src/core/script_engine.cpp
  src/core/uuid.cpp
)

set(SOURCE_RENDER
  src/render/shader.cpp
  src/render/texture.cpp
  src/render/renderer.cpp
  src/render/render_pipeline.cpp
  src/render/render_pass.cpp
  src/render/frame_buffer.cpp
  src/render/rhi/rhi.cpp
  src/render/rhi/opengl_rhi.cpp
  src/render/rhi/vulkan_rhi.cpp
  src/render/rhi/resource_backend.cpp
  src/render/rhi/opengl_resource_backend.cpp
  src/render/rhi/vulkan_resource_backend.cpp
)

set(SOURCE_SCENE
  src/scene/scene.cpp
)

set(SOURCE_UTILS
  src/utils/profiler.cpp
)

add_library(engine
  ${SOURCE_CORE}
  ${SOURCE_RENDER}
  ${SOURCE_SCENE}
  ${SOURCE_UTILS}
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

if(Vulkan_FOUND)
  target_compile_definitions(engine PRIVATE MENGINE_HAS_VULKAN=1)
endif()

target_link_libraries(engine
  glfw
  lua
  imgui
  ImGuizmo
)

if(Vulkan_FOUND)
  target_link_libraries(engine Vulkan::Vulkan)
endif()
