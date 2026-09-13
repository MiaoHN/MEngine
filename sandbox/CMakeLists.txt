# Two standalone hosts, sharing the engine's public API:
#   sandbox3d - glTF helmet demo (PBR lights, shadows, post-processing)
#   sandbox2d - sprite demo (orthographic camera, texture-sheet animation)
# Both double as scene players: `--scene <path>` runs a scene saved by the
# editor (the editor's Launch button picks the one matching the scene).
set(SANDBOX_TARGETS sandbox3d sandbox2d)

add_executable(sandbox3d src/sandbox_3d.cpp)
add_executable(sandbox2d src/sandbox_2d.cpp)

foreach(target IN LISTS SANDBOX_TARGETS)
  target_include_directories(${target}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/engine/src
    src
  )

  target_include_directories(${target} SYSTEM PRIVATE
    ${PROJECT_SOURCE_DIR}/deps/entt/single_include
    ${PROJECT_SOURCE_DIR}/deps/glm
    ${PROJECT_SOURCE_DIR}/deps/glad/include
    ${PROJECT_SOURCE_DIR}/deps/stb
    ${PROJECT_SOURCE_DIR}/deps/imgui
    ${PROJECT_SOURCE_DIR}/deps/imgui/backends
    ${PROJECT_SOURCE_DIR}/deps/ImGuizmo
  )

  target_link_libraries(${target}
    engine
  )

  mengine_enable_warnings(${target})

  # Each sandbox gets its own output directory so the POST_BUILD asset copies
  # never race with each other (the two targets build in parallel).
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${target}
  )

  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:${target}>/assets
  )
endforeach()

add_custom_command(TARGET sandbox3d POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/res
  $<TARGET_FILE_DIR:sandbox3d>/res
)

# (The sandboxes have no editor-style res/ contents beyond the fonts; the
# editor keeps its own POST_BUILD copy of res/.)