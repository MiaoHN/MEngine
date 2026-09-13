#version 460 core

layout(location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 proj_view;

out vec3 v_world_pos;

void main() {
  vec4 world   = model * vec4(aPos, 1.0);
  v_world_pos  = world.xyz;
  gl_Position  = proj_view * world;
}
