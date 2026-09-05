#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
// Per-instance model matrix (engine convention, locations 3..6).
layout(location = 3) in mat4 aInstanceModel;

uniform mat4 proj_view;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main() {
  mat4 model = aInstanceModel;
  FragPos     = vec3(model * vec4(aPos, 1.0));
  Normal      = normalize(mat3(transpose(inverse(model))) * aNormal);
  TexCoord    = aTexCoord;
  gl_Position = proj_view * vec4(FragPos, 1.0);
}
