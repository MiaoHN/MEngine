#version 460 core

layout(location = 0) in vec3 aPos;
// Per-instance model matrix (engine convention, locations 3..6).
layout(location = 3) in mat4 aInstanceModel;

uniform mat4 light_view_proj;

void main() {
  gl_Position = light_view_proj * aInstanceModel * vec4(aPos, 1.0);
}
