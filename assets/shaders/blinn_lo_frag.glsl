#version 460 core

// LearnOpenGL-exact lighting fragment shader ("blinn_lo"), used by the 1:1 LO
// example ports (2.2 / 3.1 / 4.2 / 6.multiple_lights). It reproduces LO's
// per-light ambient/diffuse/specular split with a Phong (reflect) specular
// term that is NOT gated by NdotL, sampling the material's specular map (or an
// explicit specular color) - term-for-term matching LO's *.fs shaders:
//
//   directional: ambient = dir.ambient * albedo
//                diffuse  = dir.diffuse * NdotL * albedo
//                specular = dir.specular * pow(VdotR, shininess) * specSample
//   point/spot:  same, times attenuation (only when lo_attenuation is set, as
//                in 6.multiple_lights; 2.2/3.1/4.2 have no attenuation)
//
// Material mapping: albedo = base_color_factor.rgb (* albedo map), specular
// sample = specular_map texel (else u_material_specular_color, else a scalar
// specular_intensity grey), shininess = material_shininess.
//
// The engine renders this into an HDR target and (for LO scenes) composites it
// with raw clamp (no ACES/gamma) via Scene::SetLinearOutput, so the result
// matches LO's untonemapped output.

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D albedo_map;
uniform sampler2D specular_map;

uniform int  has_albedo_map   = 0;
uniform int  has_specular_map = 0;

uniform vec4  base_color_factor   = vec4(1.0);
uniform float specular_intensity  = 1.0;
uniform float material_shininess  = 32.0;
uniform vec3  u_material_specular_color      = vec3(1.0);
uniform int   u_material_has_specular_color  = 0;
uniform int   u_material_unlit    = 0;  // emissive light-source cubes

uniform vec3 view_pos;
uniform vec3 light_dir   = normalize(vec3(-0.2, -1.0, -0.3));
uniform vec3 light_ambient  = vec3(0.05);
uniform vec3 light_diffuse  = vec3(0.4);
uniform vec3 light_specular = vec3(0.5);

#define MAX_POINT_LIGHTS 8
uniform int   point_light_count = 0;
uniform vec3  point_light_positions[MAX_POINT_LIGHTS];
uniform vec3  point_light_ambients[MAX_POINT_LIGHTS];
uniform vec3  point_light_diffuses[MAX_POINT_LIGHTS];
uniform vec3  point_light_speculars[MAX_POINT_LIGHTS];
uniform float point_light_constants[MAX_POINT_LIGHTS];
uniform float point_light_linears[MAX_POINT_LIGHTS];
uniform float point_light_quadratics[MAX_POINT_LIGHTS];
uniform int   point_light_lo_attenuation[MAX_POINT_LIGHTS];

#define MAX_SPOT_LIGHTS 4
uniform int   spot_light_count = 0;
uniform vec3  spot_light_positions[MAX_SPOT_LIGHTS];
uniform vec3  spot_light_directions[MAX_SPOT_LIGHTS];
uniform vec3  spot_light_ambients[MAX_SPOT_LIGHTS];
uniform vec3  spot_light_diffuses[MAX_SPOT_LIGHTS];
uniform vec3  spot_light_speculars[MAX_SPOT_LIGHTS];
uniform float spot_light_cutoffs[MAX_SPOT_LIGHTS];
uniform float spot_light_outer_cutoffs[MAX_SPOT_LIGHTS];
uniform float spot_light_constants[MAX_SPOT_LIGHTS];
uniform float spot_light_linears[MAX_SPOT_LIGHTS];
uniform float spot_light_quadratics[MAX_SPOT_LIGHTS];
uniform int   spot_light_lo_attenuation[MAX_SPOT_LIGHTS];

/// @brief Material specular sample: specular map texel when present, else the
/// explicit specular color, else the scalar specular intensity as grey.
vec3 SpecularSample() {
  if (has_specular_map == 1) {
    return texture(specular_map, TexCoord).rgb;
  }
  if (u_material_has_specular_color == 1) {
    return u_material_specular_color;
  }
  return vec3(specular_intensity);
}

/// @brief LO Phong specular power: pow(max(dot(V, reflect(-L, N)), 0), s).
float PhongSpec(vec3 N, vec3 L, vec3 V) {
  vec3 R = reflect(-L, N);
  return pow(max(dot(V, R), 0.0), max(material_shininess, 1.0));
}

void main() {
  vec3 albedo = has_albedo_map == 1 ? texture(albedo_map, TexCoord).rgb : vec3(1.0);
  albedo *= base_color_factor.rgb;

  // Emissive light-source cubes output their color directly (LO light_cube).
  if (u_material_unlit == 1) {
    FragColor = vec4(albedo, base_color_factor.a);
    return;
  }

  vec3 N = normalize(Normal);
  vec3 V = normalize(view_pos - FragPos);
  vec3 spec_sample = SpecularSample();

  // --- directional light (LO CalcDirLight) ---
  {
    vec3  L    = normalize(-light_dir);
    float diff = max(dot(N, L), 0.0);
    vec3  ambient  = light_ambient * albedo;
    vec3  diffuse  = light_diffuse * diff * albedo;
    vec3  specular = light_specular * PhongSpec(N, L, V) * spec_sample;
    FragColor = vec4(ambient + diffuse + specular, base_color_factor.a);
  }

  // --- point lights (LO CalcPointLight) ---
  for (int i = 0; i < point_light_count && i < MAX_POINT_LIGHTS; ++i) {
    vec3  L    = point_light_positions[i] - FragPos;
    float dist = length(L);
    L /= max(dist, 0.0001);
    float diff = max(dot(N, L), 0.0);

    // Attenuation only where LO sets c/l/q (6.multiple_lights); the earlier
    // tutorials (2.2 / 3.1 / 4.2) have none.
    float att = 1.0;
    if (point_light_lo_attenuation[i] == 1) {
      att = 1.0 / max(point_light_constants[i] + point_light_linears[i] * dist +
                          point_light_quadratics[i] * dist * dist, 0.0001);
    }

    vec3 ambient  = point_light_ambients[i] * albedo;
    vec3 diffuse  = point_light_diffuses[i] * diff * albedo;
    vec3 specular = point_light_speculars[i] * PhongSpec(N, L, V) * spec_sample;
    FragColor += vec4((ambient + diffuse + specular) * att, 0.0);
  }

  // --- spot lights (LO CalcSpotLight) ---
  for (int i = 0; i < spot_light_count && i < MAX_SPOT_LIGHTS; ++i) {
    vec3  L    = spot_light_positions[i] - FragPos;
    float dist = length(L);
    L /= max(dist, 0.0001);
    float diff = max(dot(N, L), 0.0);

    float theta   = dot(L, normalize(-spot_light_directions[i]));
    float epsilon = spot_light_cutoffs[i] - spot_light_outer_cutoffs[i];
    float cone    = clamp((theta - spot_light_outer_cutoffs[i]) / max(epsilon, 0.0001), 0.0, 1.0);

    float att = 1.0;
    if (spot_light_lo_attenuation[i] == 1) {
      att = 1.0 / max(spot_light_constants[i] + spot_light_linears[i] * dist +
                          spot_light_quadratics[i] * dist * dist, 0.0001);
    }

    vec3 ambient  = spot_light_ambients[i] * albedo;
    vec3 diffuse  = spot_light_diffuses[i] * diff * albedo;
    vec3 specular = spot_light_speculars[i] * PhongSpec(N, L, V) * spec_sample;
    FragColor += vec4((ambient + diffuse + specular) * att * cone, 0.0);
  }
}
