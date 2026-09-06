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
uniform sampler2D normal_map;    // optional (LO 4.normal_mapping)
uniform sampler2D specular_map;

uniform int  has_albedo_map   = 0;
uniform int  has_normal_map   = 0;
uniform int  has_specular_map = 0;

uniform vec4  base_color_factor   = vec4(1.0);
uniform float specular_intensity  = 1.0;
uniform float material_shininess  = 32.0;
uniform vec3  u_material_specular_color      = vec3(1.0);
uniform int   u_material_has_specular_color  = 0;
uniform int   u_material_unlit    = 0;  // emissive light-source cubes
uniform int   u_lo_blinn_spec     = 0;  // LO 4 uses Blinn(halfway), other LO .fs use Phong(reflect)

uniform vec3 view_pos;
uniform vec3 light_dir   = normalize(vec3(-0.2, -1.0, -0.3));
uniform vec3 light_ambient  = vec3(0.05);
uniform vec3 light_diffuse  = vec3(0.4);
uniform vec3 light_specular = vec3(0.5);

// Optional directional shadow (engine shadow map, uploaded for every material).
// Only applied when u_lo_dir_shadow == 1 (LO 3.1.3.shadow_mapping).
uniform sampler2D shadow_map;
uniform mat4      light_view_proj;
uniform float     shadow_map_size   = 2048.0;
uniform float     shadow_pcf_radius = 2.0;
uniform int       u_lo_dir_shadow   = 0;

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
uniform int   spot_light_lo_flashlight[MAX_SPOT_LIGHTS];

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

/// @brief LO specular term. Most LO .fs use Phong reflect: pow(dot(V,R), s).
/// LO 4.normal_mapping instead uses Blinn-Phong halfway: pow(dot(N,H), s).
float SpecTerm(vec3 N, vec3 L, vec3 V) {
  float s = max(material_shininess, 1.0);
  if (u_lo_blinn_spec == 1) {
    vec3 H = normalize(L + V);
    return pow(max(dot(N, H), 0.0), s);
  }
  vec3 R = reflect(-L, N);
  return pow(max(dot(V, R), 0.0), s);
}

/// @brief Directional shadow: returns the fraction of the fragment that is LIT
/// (1.0 = fully lit), sampled from the engine's directional shadow map with
/// 5x5 PCF. Mirrors the classic blinn/pbr ShadowCalculation.
float DirShadowLit(vec3 frag_pos_world, vec3 N, vec3 L) {
  vec4 clip = light_view_proj * vec4(frag_pos_world, 1.0);
  vec3 proj = clip.xyz / clip.w;
  proj      = proj * 0.5 + 0.5;
  if (proj.z > 1.0) {
    return 1.0;
  }
  float current = proj.z;
  float bias    = max(0.002 * (1.0 - dot(N, L)), 0.0005) * max(shadow_pcf_radius, 1.0) * 2.0;

  vec2  texel  = 1.0 / vec2(shadow_map_size);
  float lit    = 0.0;
  for (int x = -2; x <= 2; ++x) {
    for (int y = -2; y <= 2; ++y) {
      vec2 uv = proj.xy + vec2(x, y) * texel * shadow_pcf_radius;
      if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        lit += 1.0;
        continue;
      }
      float closest = texture(shadow_map, uv).r;
      lit += (current - bias > closest) ? 0.0 : 1.0;
    }
  }
  return lit / 25.0;
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
  // Optional normal mapping (LO 4.normal_mapping): world-space TBN built from
  // screen-space derivatives (same as the pbr shader; no tangent attributes).
  if (has_normal_map == 1) {
    vec3 n    = texture(normal_map, TexCoord).rgb * 2.0 - 1.0;
    vec3 dp1  = dFdx(FragPos);
    vec3 dp2  = dFdy(FragPos);
    vec2 duv1 = dFdx(TexCoord);
    vec2 duv2 = dFdy(TexCoord);
    vec3 T    = normalize(dp1 * duv2.t - dp2 * duv1.t);
    vec3 B    = normalize(cross(N, T));
    mat3 TBN  = mat3(T, B, N);
    N         = normalize(TBN * n);
  }
  vec3 V = normalize(view_pos - FragPos);
  vec3 spec_sample = SpecularSample();

  // --- directional light (LO CalcDirLight; optional engine shadow) ---
  {
    vec3  L    = normalize(-light_dir);
    float diff = max(dot(N, L), 0.0);
    vec3  ambient = light_ambient * albedo;
    vec3  direct  = light_diffuse * diff * albedo + light_specular * SpecTerm(N, L, V) * spec_sample;
    if (u_lo_dir_shadow == 1) {
      direct *= DirShadowLit(FragPos, N, L);  // LO 3.1.3: (1 - shadow)
    }
    FragColor = vec4(ambient + direct, base_color_factor.a);
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
    vec3 specular = point_light_speculars[i] * SpecTerm(N, L, V) * spec_sample;
    FragColor += vec4((ambient + diffuse + specular) * att, 0.0);
  }

  // --- spot lights ---
  // Two LO spot flavours:
  //  * lo_flashlight (5.3/5.4.light_casters): ambient lights EVERYTHING
  //    (unattenuated, outside the cone too = LO's else branch); diffuse +
  //    specular get cone-intensity * attenuation.
  //  * else (6.multiple_lights CalcSpotLight): ambient + diffuse + specular all
  //    coned and attenuated.
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
    vec3 specular = spot_light_speculars[i] * SpecTerm(N, L, V) * spec_sample;

    if (spot_light_lo_flashlight[i] == 1) {
      FragColor += vec4(ambient + (diffuse + specular) * cone * att, 0.0);
    } else {
      FragColor += vec4((ambient + diffuse + specular) * att * cone, 0.0);
    }
  }
}
