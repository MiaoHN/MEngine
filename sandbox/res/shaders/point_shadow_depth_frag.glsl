#version 460 core

in vec3 FragPos;

uniform vec3  light_pos;
uniform float far_plane;

void main() {
  float light_distance = length(FragPos - light_pos);
  // Normalize by the light's reach so the value fits in [0, 1] depth range.
  gl_FragDepth = light_distance / far_plane;
}
