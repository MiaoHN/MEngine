#version 460 core

// Screen-space ambient occlusion.
//
// The G-buffer stores VIEW-space positions (camera looks down -Z, so geometry
// in front of the camera has a NEGATIVE z and "farther" = smaller z) and
// view-space normals. The fragment shader reconstructs, for every screen
// pixel, a tangent-space hemisphere sample kernel around the surface point and
// compares each sample's depth against the G-buffer depth: a sample is
// occluded when real geometry is found CLOSER to the camera than the sample
// (sampleDepth >= samplePos.z). Background / sky pixels and samples that read
// no geometry are treated as unoccluded.

in vec2 uv;
out float FragColor;

uniform sampler2D g_position;
uniform sampler2D g_normal;
uniform sampler2D tex_noise;

uniform vec3  samples[64];
uniform mat4  proj;
uniform vec2  noise_scale;
uniform float radius;
uniform float bias;

const int SAMPLE_COUNT = 32;

void main() {
  vec3 frag_pos = texture(g_position, uv).xyz;

  // No geometry here (background / skybox) -> fully lit.
  if (length(frag_pos) < 1e-3) {
    FragColor = 1.0;
    return;
  }

  vec3 normal     = normalize(texture(g_normal, uv).rgb);
  vec3 random_vec = normalize(texture(tex_noise, uv * noise_scale).xyz);

  vec3 tangent   = normalize(random_vec - normal * dot(random_vec, normal));
  vec3 bitangent = cross(normal, tangent);
  mat3 TBN       = mat3(tangent, bitangent, normal);

  float occlusion = 0.0;
  for (int i = 0; i < SAMPLE_COUNT; ++i) {
    vec3 sample_pos = frag_pos + TBN * samples[i] * radius;

    vec4 offset = proj * vec4(sample_pos, 1.0);
    offset.xyz /= offset.w;
    offset.xyz  = offset.xyz * 0.5 + 0.5;

    vec3 sample_geom = texture(g_position, offset.xy).xyz;
    // A sample that lands outside geometry sees nothing -> not occluded.
    if (length(sample_geom) < 1e-3) {
      continue;
    }

    float sample_depth = sample_geom.z;
    float range_check  = smoothstep(0.0, 1.0, radius / abs(frag_pos.z - sample_depth));
    occlusion += (sample_depth >= sample_pos.z + bias ? 1.0 : 0.0) * range_check;
  }

  FragColor = 1.0 - occlusion / float(SAMPLE_COUNT);
}
