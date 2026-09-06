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

# Target naming: ex_<LO-top-chapter>_<LO-demo-number>_<name>
#   e.g. ex_2_6_multiple_lights = LearnOpenGL src/2.lighting/6.multiple_lights
#        ex_5_4_normal_mapping  = LearnOpenGL src/5.advanced_lighting/4.normal_mapping
# The exact LO source file is also named in a header comment of each .cpp.
set(MENGINE_EXAMPLE_DEMOS
  ex_2_1_colors                  # 2.lighting/1.colors
  ex_2_2_basic_lighting          # 2.lighting/2.2.basic_lighting_specular (PBR)
  ex_2_2_blinn_lighting          # 2.lighting/2.2 ... on the Blinn-Phong pipeline
  ex_2_3_materials               # 2.lighting/3.1.materials
  ex_2_4_lighting_maps           # 2.lighting/4.1.lighting_maps_diffuse_map
  ex_2_5_light_casters           # 2.lighting/5.3.light_casters_spot
  ex_2_6_multiple_lights         # 2.lighting/6.multiple_lights
)

# 5.advanced_lighting
list(APPEND MENGINE_EXAMPLE_DEMOS
  ex_5_3_shadow_mapping          # 5.advanced_lighting/3.1.x shadow_mapping + 3.2 point_shadows
  ex_5_4_normal_mapping          # 5.advanced_lighting/4.normal_mapping
  ex_5_6_hdr_bloom               # 5.advanced_lighting/6.hdr + 7.bloom
  ex_5_8_deferred_shading        # 5.advanced_lighting/8.1.deferred_shading (forward eq., 32 lights)
  ex_5_9_ssao                    # 5.advanced_lighting/9.ssao (engine SSAO pass, Space toggles)
)

# 6.pbr
list(APPEND MENGINE_EXAMPLE_DEMOS
  ex_6_1_1_pbr_lighting          # 6.pbr/1.1.lighting (engine PBR, 7x7 metal/rough matrix)
  ex_6_1_2_pbr_lighting_textured # 6.pbr/1.2.lighting_textured (rusted_iron, engine PBR)
  ex_6_2_2_ibl_specular_textured # 6.pbr/2.2.2.ibl_specular_textured (5 materials, newport_loft IBL)
  ex_6_2_cerberus                # user Cerberus model in the LO 6.2 newport_loft IBL scene
)

# Utility: engine model viewer (user asset, e.g. Cerberus GLB)
list(APPEND MENGINE_EXAMPLE_DEMOS
  ex_model_viewer
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
