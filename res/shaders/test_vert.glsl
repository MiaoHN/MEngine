#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 Normal;
out vec3 ViewPos;
out vec3 FragPos;

layout(std140, binding = 0) uniform camera {
  mat4 viewProjection;
  vec3 cameraPosition;
};

uniform mat4 model;

void main() {
  gl_Position = viewProjection * model * vec4(aPos, 1.0f);
  TexCoord    = aTexCoord;
  // Normal      = vec3(model * vec4(aNormal, 1.0f));
  Normal  = mat3(transpose(inverse(model))) * aNormal;
  FragPos = vec3(model * vec4(aPos, 1.0f));
  ViewPos = cameraPosition;
}