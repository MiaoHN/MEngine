#version 460 core

// Classic Blinn-Phong lighting (the LearnOpenGL pipeline), as an alternative
// to the PBR shader. It reads the SAME uniforms the renderer uploads for pbr
// (base color / albedo+normal maps / shadows / SSAO / point & spot light
// arrays), so a material simply picks its shader: "pbr" or "blinn".
//
// Mapping vs pbr:
//   base_color (+ albedo map) = diffuse color
//   specular_intensity        = specular strength (white specular)
//   material_shininess        = Blinn exponent (default 32)
//   ibl_intensity             = small global ambient strength
//   metallic_factor           = ignored (no metals in Blinn)
//   roughness_factor          = ignored
// The engine still renders in HDR and tone maps / gamma-corrects afterwards,
// so this shader outputs linear HDR color like pbr does.

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D albedo_map;
uniform sampler2D normal_map;
uniform sampler2D metallic_roughness_map;  // unused (kept so batching matches)
uniform sampler2D ao_map;
uniform sampler2D height_map;  // parallax height map (slot 15)

uniform int  has_albedo_map             = 0;
uniform int  has_normal_map             = 0;
uniform int  has_metallic_roughness_map = 0;
uniform int  has_ao_map                 = 0;
uniform int  has_height_map             = 0;

uniform vec4  base_color_factor   = vec4(1.0);
uniform float metallic_factor     = 1.0;   // ignored
uniform float roughness_factor    = 1.0;   // ignored
uniform float specular_intensity  = 1.0;
uniform float material_shininess  = 32.0;
uniform float height_scale        = 0.0;   // parallax strength (0 = off)
uniform int   u_render_mode       = 0;     // 0 = lit, 1 = unlit albedo
uniform int   u_material_unlit    = 0;     // per-material emissive (light cubes)

uniform vec3 view_pos;
uniform vec3 light_dir   = normalize(vec3(-0.3, -1.0, -0.4));
uniform vec3 light_color = vec3(2.5);

// Additional (unshadowed) directional lights - the engine's multi-directional
// path. The first/primary directional light is the one that casts shadows.
#define MAX_DIR_EXTRA 4
uniform int   dir_extra_count = 0;
uniform vec3  dir_extra_dir[MAX_DIR_EXTRA];
uniform vec3  dir_extra_color[MAX_DIR_EXTRA];

uniform sampler2D shadow_map;
uniform mat4      light_view_proj;
uniform float     shadow_map_size     = 2048.0;
uniform float     shadow_pcf_radius   = 2.0;

uniform float ibl_intensity = 1.0;

uniform sampler2D ssao_map;
uniform int       ssao_enabled   = 0;
uniform vec2      viewport_size  = vec2(1600.0, 900.0);

#define MAX_POINT_LIGHTS 8
uniform int   point_light_count = 0;
uniform vec3  point_light_positions[MAX_POINT_LIGHTS];
uniform vec3  point_light_colors[MAX_POINT_LIGHTS];
uniform float point_light_intensities[MAX_POINT_LIGHTS];
uniform float point_light_radii[MAX_POINT_LIGHTS];
uniform float point_light_constants[MAX_POINT_LIGHTS];
uniform float point_light_linears[MAX_POINT_LIGHTS];
uniform float point_light_quadratics[MAX_POINT_LIGHTS];
uniform int   point_light_lo_attenuation[MAX_POINT_LIGHTS];
uniform samplerCube point_light_shadow_maps[MAX_POINT_LIGHTS];
uniform int   point_light_has_shadow[MAX_POINT_LIGHTS];
uniform float point_light_far_planes[MAX_POINT_LIGHTS];
uniform float point_shadow_size = 512.0;  // cube face resolution of the point shadow maps

#define MAX_SPOT_LIGHTS 4
uniform int   spot_light_count = 0;
uniform vec3  spot_light_positions[MAX_SPOT_LIGHTS];
uniform vec3  spot_light_directions[MAX_SPOT_LIGHTS];
uniform vec3  spot_light_colors[MAX_SPOT_LIGHTS];
uniform float spot_light_intensities[MAX_SPOT_LIGHTS];
uniform float spot_light_ranges[MAX_SPOT_LIGHTS];
uniform float spot_light_cutoffs[MAX_SPOT_LIGHTS];
uniform float spot_light_outer_cutoffs[MAX_SPOT_LIGHTS];
uniform float spot_light_constants[MAX_SPOT_LIGHTS];
uniform float spot_light_linears[MAX_SPOT_LIGHTS];
uniform float spot_light_quadratics[MAX_SPOT_LIGHTS];
uniform int   spot_light_lo_attenuation[MAX_SPOT_LIGHTS];

float ShadowCalculation(vec3 frag_pos_world, vec3 N, vec3 L) {
  vec4 clip = light_view_proj * vec4(frag_pos_world, 1.0);
  vec3 proj = clip.xyz / clip.w;
  proj      = proj * 0.5 + 0.5;
  if (proj.z > 1.0) {
    return 1.0;
  }
  float current = proj.z;
  float bias    = max(0.002 * (1.0 - dot(N, L)), 0.0005) * max(shadow_pcf_radius, 1.0) * 2.0;

  vec2  texel  = 1.0 / vec2(shadow_map_size);
  float shadow = 0.0;
  for (int x = -2; x <= 2; ++x) {
    for (int y = -2; y <= 2; ++y) {
      vec2 uv = proj.xy + vec2(x, y) * texel * shadow_pcf_radius;
      if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        shadow += 1.0;
        continue;
      }
      float closest = texture(shadow_map, uv).r;
      shadow += (current - bias > closest) ? 0.0 : 1.0;
    }
  }
  return shadow / 25.0;
}

float PointShadowCalculation(int light_index, vec3 light_pos, vec3 N, vec3 L) {
  vec3  frag_to_light = FragPos - light_pos;
  float current       = length(frag_to_light);
  if (current <= 1e-5) {
    return 1.0;
  }
  vec3 dir = frag_to_light / current;

  // PCF: perturb the cube sampling direction in the plane perpendicular to the
  // light ray. A cubemap texel subtends ~2/N radians, so spread the 5x5 taps by
  // shadow_pcf_radius texels (returns the lit fraction, 1.0 = fully lit).
  vec3  up   = abs(dir.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
  vec3  t    = normalize(cross(up, dir));
  vec3  b    = cross(dir, t);
  float step = (2.0 / point_shadow_size) * max(shadow_pcf_radius, 1.0);

  float bias = max(0.05 * (1.0 - dot(N, L)), 0.005);
  float lit  = 0.0;
  for (int x = -2; x <= 2; ++x) {
    for (int y = -2; y <= 2; ++y) {
      vec3  sd      = normalize(dir + (t * float(x) + b * float(y)) * step);
      float closest = texture(point_light_shadow_maps[light_index], sd).r * point_light_far_planes[light_index];
      lit += (current - bias > closest) ? 0.0 : 1.0;
    }
  }
  return lit / 25.0;
}

float DistanceAttenuation(vec3 light_pos, float radius, bool lo_attenuation, float constant, float linear,
                          float quadratic) {
  vec3  L        = light_pos - FragPos;
  float distance = length(L);
  if (lo_attenuation) {
    // LearnOpenGL: 1 / (constant + linear*d + quadratic*d^2)
    return 1.0 / max(constant + linear * distance + quadratic * distance * distance, 0.0001);
  }
  float att = clamp(1.0 - pow(distance / max(radius, 0.001), 4.0), 0.0, 1.0);
  att *= att;
  return att / max(distance * distance, 0.001);
}

/// @brief Parallax occlusion mapping - see pbr_frag.glsl for the details.
vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir) {
  const float minLayers = 8.0;
  const float maxLayers = 32.0;
  float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));
  float layerDepth       = 1.0 / numLayers;
  float currentLayerDepth = 0.0;
  vec2  P                = viewDir.xy / max(abs(viewDir.z), 0.0001) * height_scale;
  vec2  deltaTexCoords   = P / numLayers;

  vec2  currentTexCoords     = texCoords;
  float currentDepthMapValue = texture(height_map, currentTexCoords).r;
  int   guard                = 0;
  while (currentLayerDepth < currentDepthMapValue && guard < 64) {
    currentTexCoords -= deltaTexCoords;
    currentDepthMapValue = texture(height_map, currentTexCoords).r;
    currentLayerDepth += layerDepth;
    ++guard;
  }

  vec2  prevTexCoords = currentTexCoords + deltaTexCoords;
  float afterDepth    = currentDepthMapValue - currentLayerDepth;
  float beforeDepth   = texture(height_map, prevTexCoords).r - currentLayerDepth + layerDepth;
  float weight        = clamp(afterDepth / max(afterDepth - beforeDepth, 0.0001), 0.0, 1.0);
  return prevTexCoords * weight + currentTexCoords * (1.0 - weight);
}

void main() {
  vec2 uv = TexCoord;
  // Parallax mapping (lit pass only) - see pbr_frag.glsl for the details.
  if (has_height_map == 1 && height_scale > 0.0 && u_render_mode != 1 && u_material_unlit != 1) {
    vec2 duv1 = dFdx(TexCoord);
    vec2 duv2 = dFdy(TexCoord);
    if (dot(duv1, duv1) > 1e-8 && dot(duv2, duv2) > 1e-8) {
      vec3  N_geom = normalize(Normal);
      vec3  dp1    = dFdx(FragPos);
      vec3  dp2    = dFdy(FragPos);
      vec3  T      = normalize(dp1 * duv2.t - dp2 * duv1.t);
      vec3  B      = normalize(cross(N_geom, T));
      mat3  TBN    = mat3(T, B, N_geom);
      vec3  V_world = normalize(view_pos - FragPos);
      vec3  view_tan = transpose(TBN) * V_world;  // world -> tangent space
      uv = ParallaxMapping(TexCoord, view_tan);
      if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        discard;
      }
    }
  }

  vec3 albedo = has_albedo_map == 1 ? texture(albedo_map, uv).rgb : vec3(1.0);
  albedo *= base_color_factor.rgb;

  if (u_render_mode == 1 || u_material_unlit == 1) {  // Unlit / emissive
    FragColor = vec4(albedo, base_color_factor.a);
    return;
  }

  vec3 N = normalize(Normal);
  if (has_normal_map == 1) {
    vec3 n    = texture(normal_map, uv).rgb * 2.0 - 1.0;
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

  float ao = has_ao_map == 1 ? texture(ao_map, uv).r : 1.0;
  if (ssao_enabled == 1) {
    ao *= texture(ssao_map, gl_FragCoord.xy / viewport_size).r;
  }

  // Small global ambient (LearnOpenGL uses a tiny per-light ambient too; we
  // fold it into one term driven by ibl_intensity).
  vec3 ambient = albedo * clamp(ibl_intensity, 0.0, 1.0) * 0.35;

  const float kSpec = specular_intensity;
  const float kShin = max(material_shininess, 1.0);

  vec3 L = normalize(-light_dir);
  float NdotL = max(dot(N, L), 0.0);
  if (NdotL > 0.0) {
    vec3 H = normalize(V + L);
    vec3 diffuse  = albedo * light_color * NdotL;
    vec3 specular = light_color * kSpec *
                    pow(max(dot(N, H), 0.0), kShin) * NdotL;
    vec3 direct = (diffuse + specular) * ShadowCalculation(FragPos, N, L);
    // (direction lights are not attenuated; LearnOpenGL applies the same)
    ambient += direct;
  }

  // Additional (unshadowed) directional lights accumulate on top of the
  // shadowed primary.
  for (int i = 0; i < dir_extra_count && i < MAX_DIR_EXTRA; ++i) {
    vec3  Le   = normalize(-dir_extra_dir[i]);
    float NdLe = max(dot(N, Le), 0.0);
    if (NdLe <= 0.0) {
      continue;
    }
    vec3 H = normalize(V + Le);
    ambient += albedo * dir_extra_color[i] * NdLe +
               dir_extra_color[i] * kSpec * pow(max(dot(N, H), 0.0), kShin) * NdLe;
  }

  for (int i = 0; i < point_light_count && i < MAX_POINT_LIGHTS; ++i) {
    vec3  LP   = point_light_positions[i] - FragPos;
    float dist = length(LP);
    vec3  Lp   = LP / max(dist, 0.0001);
    float NdotLp = max(dot(N, Lp), 0.0);
    if (NdotLp <= 0.0) {
      continue;
    }
    float att = DistanceAttenuation(point_light_positions[i], point_light_radii[i],
                                    point_light_lo_attenuation[i] == 1, point_light_constants[i],
                                    point_light_linears[i], point_light_quadratics[i]);
    vec3  H   = normalize(V + Lp);
    vec3  diffuse  = albedo * point_light_colors[i] * point_light_intensities[i] * NdotLp;
    vec3  specular = point_light_colors[i] * point_light_intensities[i] * kSpec *
                     pow(max(dot(N, H), 0.0), kShin) * NdotLp;
    float shadow = (point_light_has_shadow[i] == 1)
                       ? PointShadowCalculation(i, point_light_positions[i], N, Lp)
                       : 1.0;
    ambient += (diffuse + specular) * att * shadow;
  }

  for (int i = 0; i < spot_light_count && i < MAX_SPOT_LIGHTS; ++i) {
    vec3  Ls   = spot_light_positions[i] - FragPos;
    float dist = length(Ls);
    Ls /= max(dist, 0.0001);
    float NdotLs = max(dot(N, Ls), 0.0);
    if (NdotLs <= 0.0) {
      continue;
    }
    float theta   = dot(-Ls, normalize(spot_light_directions[i]));
    float epsilon = spot_light_cutoffs[i] - spot_light_outer_cutoffs[i];
    float cone    = clamp((theta - spot_light_outer_cutoffs[i]) / max(epsilon, 0.0001), 0.0, 1.0);
    float att = DistanceAttenuation(spot_light_positions[i], spot_light_ranges[i],
                                    spot_light_lo_attenuation[i] == 1, spot_light_constants[i],
                                    spot_light_linears[i], spot_light_quadratics[i]) *
                cone;
    vec3  H       = normalize(V + Ls);
    vec3  diffuse  = albedo * spot_light_colors[i] * spot_light_intensities[i] * NdotLs;
    vec3  specular = spot_light_colors[i] * spot_light_intensities[i] * kSpec *
                     pow(max(dot(N, H), 0.0), kShin) * NdotLs;
    ambient += (diffuse + specular) * att;
  }

  ambient *= ao;
  // Linear HDR output; tone mapping + gamma happen in the post-process pass.
  FragColor = vec4(ambient, base_color_factor.a);
}
