#version 460 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D scene;
uniform vec2  light_pos;  // light source in screen space [0,1]
uniform float decay   = 0.97;
uniform float density = 0.6;
uniform float weight  = 0.04;
uniform int   samples = 40;

void main() {
  vec2 tex_coord = uv;
  vec2 delta     = (uv - light_pos) / float(samples) * density;

  vec4  color              = texture(scene, tex_coord);
  float illumination_decay = 1.0;

  for (int i = 0; i < samples; ++i) {
    tex_coord -= delta;
    vec4 sample_color = texture(scene, tex_coord);
    sample_color *= illumination_decay * weight;
    color += sample_color;
    illumination_decay *= decay;
  }

  FragColor = vec4(color.rgb, 1.0);
}
