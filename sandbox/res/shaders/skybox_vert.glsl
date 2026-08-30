#version 460 core

layout(location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 proj;

out vec3 tex_dir;

void main() {
  tex_dir     = aPos;
  vec4 pos    = proj * view * vec4(aPos, 1.0);
  gl_Position = pos.xyww;  // force depth to the far plane
}
