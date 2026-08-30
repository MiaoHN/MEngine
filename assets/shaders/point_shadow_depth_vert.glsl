#version 460 core

layout(location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 light_space_matrix;

out vec3 FragPos;

void main() {
  FragPos      = vec3(model * vec4(aPos, 1.0));
  gl_Position  = light_space_matrix * vec4(FragPos, 1.0);
}
