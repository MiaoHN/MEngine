add_executable(editor
  src/editor.cpp
)

target_include_directories(editor
  PRIVATE
  ${PROJECT_SOURCE_DIR}/deps/entt/single_include
  ${PROJECT_SOURCE_DIR}/deps/glm
  ${PROJECT_SOURCE_DIR}/deps/glad/include
  ${PROJECT_SOURCE_DIR}/deps/stb
  ${PROJECT_SOURCE_DIR}/deps/imgui
  ${PROJECT_SOURCE_DIR}/deps/imgui/backends
  ${PROJECT_SOURCE_DIR}/deps/ImGuizmo
  ${PROJECT_SOURCE_DIR}/engine/src
  ${PROJECT_SOURCE_DIR}/editor/src
)

target_link_libraries(editor
  engine
)

add_custom_command(TARGET editor POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/res
  $<TARGET_FILE_DIR:editor>/res
)

add_custom_command(TARGET editor POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_CURRENT_SOURCE_DIR}/res/imgui.ini
  $<TARGET_FILE_DIR:editor>/imgui.ini
)
