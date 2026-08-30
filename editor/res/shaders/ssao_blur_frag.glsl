#version 460 core

in vec2 uv;
out float FragColor;

uniform sampler2D ssao_input;

void main() {
  vec2  texel  = 1.0 / vec2(textureSize(ssao_input, 0));
  float result = 0.0;
  for (int x = -2; x < 2; ++x) {
    for (int y = -2; y < 2; ++y) {
      result += texture(ssao_input, uv + vec2(float(x), float(y)) * texel).r;
    }
  }
  FragColor = result / 16.0;
}
