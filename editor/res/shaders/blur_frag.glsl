#version 460 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D image;
uniform int       horizontal = 0;
uniform float     texel_size = 1.0 / 400.0;

void main() {
  const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
  vec2        offset    = horizontal == 1 ? vec2(texel_size, 0.0) : vec2(0.0, texel_size);

  vec3 result = texture(image, uv).rgb * weight[0];
  for (int i = 1; i < 5; ++i) {
    result += texture(image, uv + offset * float(i)).rgb * weight[i];
    result += texture(image, uv - offset * float(i)).rgb * weight[i];
  }
  FragColor = vec4(result, 1.0);
}
