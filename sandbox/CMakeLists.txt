add_executable(sandbox
  src/sandbox.cpp
)

target_include_directories(sandbox
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

target_link_libraries(sandbox
  engine
)

add_custom_command(TARGET sandbox POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/res
  $<TARGET_FILE_DIR:sandbox>/res
)

# add_custom_command(TARGET sandbox POST_BUILD
#   COMMAND ${CMAKE_COMMAND} -E copy
#   ${CMAKE_SOURCE_DIR}/sandbox/res/imgui.ini
#   $<TARGET_FILE_DIR:sandbox>/imgui.ini
# )