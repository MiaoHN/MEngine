#version 460 core

layout(location = 0) in vec3 aPos;
// Per-instance model matrix (engine convention, locations 3..6).
layout(location = 3) in mat4 aInstanceModel;

uniform mat4 light_space_matrix;

out vec3 FragPos;

void main() {
  FragPos      = vec3(aInstanceModel * vec4(aPos, 1.0));
  gl_Position  = light_space_matrix * vec4(FragPos, 1.0);
}
