# LearnOpenGL-style example scenes, one executable per test scene.
# Shared host (orbit camera + render loop) lives in example_app; each <name>.cpp
# defines its own BuildScene + CreateApplication.

add_library(example_app STATIC
  src/example_app.cpp
)

target_include_directories(example_app
  PRIVATE
  ${PROJECT_SOURCE_DIR}/engine/src
  src
)

target_include_directories(example_app SYSTEM PRIVATE
  ${PROJECT_SOURCE_DIR}/deps/entt/single_include
  ${PROJECT_SOURCE_DIR}/deps/glm
  ${PROJECT_SOURCE_DIR}/deps/glad/include
  ${PROJECT_SOURCE_DIR}/deps/stb
  ${PROJECT_SOURCE_DIR}/deps/imgui
  ${PROJECT_SOURCE_DIR}/deps/imgui/backends
  ${PROJECT_SOURCE_DIR}/deps/ImGuizmo
)

target_link_libraries(example_app
  PRIVATE
  engine
)

mengine_enable_warnings(example_app)

# LearnOpenGL "2. lighting" chapter (one exe per scene)
set(MENGINE_EXAMPLE_DEMOS
  colors                # 1.colors
  basic_lighting        # 2.x basic lighting (diffuse+specular, PBR)
  blinn_lighting        # 2.x basic lighting on the Blinn-Phong pipeline
  materials             # 3.x materials
  lighting_maps         # 4.x lighting maps (container2 diffuse texture)
  multiple_lights       # 6.multiple_lights
  light_casters         # 5.x light casters (dir/point/spot)
)

# LearnOpenGL "5. advanced_lighting" chapter
list(APPEND MENGINE_EXAMPLE_DEMOS
  shadow_mapping        # 3.x shadow mapping (directional + point cube shadows)
  hdr_bloom             # 6.hdr + 7.bloom
  normal_mapping        # 4.normal_mapping (brickwall albedo + normal)
)

function(mengine_add_example name)
  add_executable(example_${name} src/${name}.cpp)

  # Every example gets its own output folder (plus its own assets copy), so
  # POST_BUILD copies never race each other and each exe finds `assets` when
  # run from its folder (like voxel/sandbox/editor).
  set_target_properties(example_${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${name}
  )

  target_include_directories(example_${name}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/engine/src
    src
  )

  target_include_directories(example_${name} SYSTEM PRIVATE
    ${PROJECT_SOURCE_DIR}/deps/entt/single_include
    ${PROJECT_SOURCE_DIR}/deps/glm
    ${PROJECT_SOURCE_DIR}/deps/glad/include
    ${PROJECT_SOURCE_DIR}/deps/stb
    ${PROJECT_SOURCE_DIR}/deps/imgui
    ${PROJECT_SOURCE_DIR}/deps/imgui/backends
    ${PROJECT_SOURCE_DIR}/deps/ImGuizmo
  )

  target_link_libraries(example_${name}
    PRIVATE
    engine
    example_app
  )

  mengine_enable_warnings(example_${name})

  add_custom_command(TARGET example_${name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${PROJECT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:example_${name}>/assets
  )
endfunction()

foreach(demo IN LISTS MENGINE_EXAMPLE_DEMOS)
  mengine_add_example(${demo})
endforeach()
