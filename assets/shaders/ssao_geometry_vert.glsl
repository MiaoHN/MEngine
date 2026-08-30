#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

out vec3 frag_pos;
out vec3 frag_normal;

void main() {
  frag_pos    = vec3(view * model * vec4(aPos, 1.0));
  frag_normal = mat3(transpose(inverse(view * model))) * aNormal;
  gl_Position = proj * vec4(frag_pos, 1.0);
}
