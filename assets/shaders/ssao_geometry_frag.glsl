#version 460 core

in vec3 frag_pos;
in vec3 frag_normal;

layout(location = 0) out vec4 g_position;
layout(location = 1) out vec4 g_normal;

void main() {
  g_position = vec4(frag_pos, 1.0);
  g_normal   = vec4(normalize(frag_normal), 1.0);
}
