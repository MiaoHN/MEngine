#version 460 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D scene;
uniform float     threshold = 1.0;

void main() {
  vec3  color      = texture(scene, uv).rgb;
  float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
  FragColor        = brightness > threshold ? vec4(color, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
}
