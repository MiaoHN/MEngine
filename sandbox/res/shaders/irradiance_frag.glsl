#version 460 core

in vec3 tex_dir;
out vec4 FragColor;

uniform samplerCube environment;

const float PI = 3.14159265359;

void main() {
  vec3 normal = normalize(tex_dir);
  vec3 up     = vec3(0.0, 1.0, 0.0);
  vec3 right  = normalize(cross(up, normal));
  up          = normalize(cross(normal, right));

  vec3  irradiance   = vec3(0.0);
  float sample_delta = 0.025;
  float nr_samples   = 0.0;

  for (float phi = 0.0; phi < 2.0 * PI; phi += sample_delta) {
    for (float theta = 0.0; theta < 0.5 * PI; theta += sample_delta) {
      vec3 tangent_sample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
      vec3 sample_vec     = tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * normal;
      irradiance += texture(environment, sample_vec).rgb * cos(theta) * sin(theta);
      nr_samples += 1.0;
    }
  }
  irradiance = PI * irradiance / nr_samples;
  FragColor  = vec4(irradiance, 1.0);
}
