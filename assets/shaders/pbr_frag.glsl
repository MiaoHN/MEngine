#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D albedo_map;
uniform sampler2D normal_map;
uniform sampler2D metallic_roughness_map;
uniform sampler2D ao_map;

uniform int  has_albedo_map              = 0;
uniform int  has_normal_map              = 0;
uniform int  has_metallic_roughness_map  = 0;
uniform int  has_ao_map                  = 0;
uniform int  u_albedo_srgb               = 0;  // 1 = decode albedo map sRGB -> linear

uniform vec4  base_color_factor = vec4(1.0);
uniform float metallic_factor   = 1.0;
uniform float roughness_factor  = 1.0;
uniform float specular_intensity = 1.0;
uniform int   u_render_mode      = 0;  // 0 = lit PBR, 1 = unlit albedo
uniform int   u_material_unlit   = 0;  // per-material emissive (light cubes)

uniform vec3 view_pos;
uniform vec3 light_dir   = normalize(vec3(-0.3, -1.0, -0.4));
uniform vec3 light_color = vec3(2.5);

uniform sampler2D shadow_map;
uniform mat4      light_view_proj;
uniform float     shadow_map_size = 2048.0;
uniform float     shadow_pcf_radius = 2.0;

uniform samplerCube irradiance_map;
uniform samplerCube prefiltered_map;
uniform sampler2D   brdf_lut;
uniform float       max_prefilter_mip = 4.0;
uniform float       ibl_intensity     = 1.0;
uniform int         u_ibl_specular     = 1;  // 0 = diffuse-only IBL (LO 2.1.2)

uniform sampler2D ssao_map;
uniform int       ssao_enabled = 0;
uniform vec2      viewport_size = vec2(1600.0, 900.0);

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

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
  float a      = roughness * roughness;
  float a2     = a * a;
  float NdotH  = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;
  float denom  = NdotH2 * (a2 - 1.0) + 1.0;
  return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
  return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
         GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ShadowCalculation(vec3 frag_pos_world, vec3 N, vec3 L) {
  vec4 clip = light_view_proj * vec4(frag_pos_world, 1.0);
  vec3 proj = clip.xyz / clip.w;
  proj      = proj * 0.5 + 0.5;
  if (proj.z > 1.0) {
    return 1.0;
  }
  float current = proj.z;
  // The bias must grow with the PCF kernel and with the surface slope: on
  // sloped surfaces the depth varies across the sampled texels, and a fixed
  // small bias causes self-shadowing (the ground turning grey).
  float bias = max(0.002 * (1.0 - dot(N, L)), 0.0005) * max(shadow_pcf_radius, 1.0) * 2.0f;

  // Percentage-closer filtering: 5x5 taps spread by shadow_pcf_radius texels.
  // Taps outside the shadow map coverage are treated as lit (no shadow).
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

vec3 PointLightContribution(vec3 light_pos, vec3 light_color, float intensity, float radius, bool lo_attenuation,
                            float constant, float linear, float quadratic, vec3 N, vec3 V,
                            vec3 albedo, float metallic, float roughness, float specular_intensity, vec3 F0) {
  vec3  L        = light_pos - FragPos;
  float distance = length(L);
  L             = normalize(L);

  float attenuation;
  if (lo_attenuation) {
    // LearnOpenGL: 1 / (constant + linear*d + quadratic*d^2)
    attenuation = 1.0 / max(constant + linear * distance + quadratic * distance * distance, 0.0001);
  } else {
    attenuation = clamp(1.0 - pow(distance / radius, 4.0), 0.0, 1.0);
    attenuation *= attenuation;
    attenuation /= max(distance * distance, 0.001);
  }

  vec3  H   = normalize(V + L);
  float NDF = DistributionGGX(N, H, roughness);
  float G   = GeometrySmith(N, V, L, roughness);
  vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

  vec3  kS          = F;
  vec3  kD          = (1.0 - kS) * (1.0 - metallic);
  vec3  numerator   = NDF * G * F;
  float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
  vec3  specular    = numerator / denominator;

  float NdotL    = max(dot(N, L), 0.0);
  vec3  radiance = light_color * intensity * attenuation;
  return (kD * albedo / PI + specular * specular_intensity) * radiance * NdotL;
}

vec3 SpotLightContribution(vec3 light_pos, vec3 light_dir, vec3 light_color, float intensity, float range,
                           float cutoff, float outer_cutoff, bool lo_attenuation,
                           float constant, float linear, float quadratic,
                           vec3 N, vec3 V, vec3 albedo, float metallic,
                           float roughness, float specular_intensity, vec3 F0) {
  vec3  L        = light_pos - FragPos;
  float distance = length(L);
  L             = normalize(L);

  float attenuation;
  if (lo_attenuation) {
    attenuation = 1.0 / max(constant + linear * distance + quadratic * distance * distance, 0.0001);
  } else {
    attenuation = clamp(1.0 - pow(distance / range, 4.0), 0.0, 1.0);
    attenuation *= attenuation;
    attenuation /= max(distance * distance, 0.001);
  }

  float theta          = dot(-L, normalize(light_dir));
  float epsilon        = cutoff - outer_cutoff;
  float intensity_spot = clamp((theta - outer_cutoff) / max(epsilon, 0.0001), 0.0, 1.0);

  vec3  H   = normalize(V + L);
  float NDF = DistributionGGX(N, H, roughness);
  float G   = GeometrySmith(N, V, L, roughness);
  vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

  vec3  kS          = F;
  vec3  kD          = (1.0 - kS) * (1.0 - metallic);
  vec3  numerator   = NDF * G * F;
  float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
  vec3  specular    = numerator / denominator;

  float NdotL    = max(dot(N, L), 0.0);
  vec3  radiance = light_color * intensity * attenuation * intensity_spot;
  return (kD * albedo / PI + specular * specular_intensity) * radiance * NdotL;
}

void main() {
  vec3 albedo = has_albedo_map == 1 ? texture(albedo_map, TexCoord).rgb : vec3(1.0);
  albedo *= base_color_factor.rgb;
  // LearnOpenGL decodes albedo maps sRGB->linear (pow 2.2) in its PBR shaders;
  // do the same when the material opts in (SetAlbedoSRGB).
  if (u_albedo_srgb == 1) {
    albedo = pow(albedo, vec3(2.2));
  }

  if (u_render_mode == 1 || u_material_unlit == 1) {  // Unlit / emissive
    FragColor = vec4(albedo, base_color_factor.a);
    return;
  }

  float metallic  = metallic_factor;
  float roughness = roughness_factor;
  if (has_metallic_roughness_map == 1) {
    vec3 mr  = texture(metallic_roughness_map, TexCoord).rgb;
    roughness *= mr.g;  // green = roughness
    metallic  *= mr.b;  // blue = metallic
  }
  roughness = clamp(roughness, 0.04, 1.0);
  metallic  = clamp(metallic, 0.0, 1.0);

  float ao = has_ao_map == 1 ? texture(ao_map, TexCoord).r : 1.0;

  vec3 N = normalize(Normal);
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
  vec3 L = normalize(-light_dir);
  vec3 H = normalize(V + L);

  vec3 F0 = mix(vec3(0.04), albedo, metallic);
  vec3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);

  float NDF = DistributionGGX(N, H, roughness);
  float G   = GeometrySmith(N, V, L, roughness);

  vec3  kS          = F;
  vec3  kD          = (1.0 - kS) * (1.0 - metallic);
  vec3  numerator   = NDF * G * F;
  float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
  vec3  specular    = numerator / denominator;

  float NdotL  = max(dot(N, L), 0.0);
  vec3  direct = (kD * albedo / PI + specular * specular_intensity) * light_color * NdotL;
  direct *= ShadowCalculation(FragPos, N, L);

  // Image-based lighting: diffuse from the irradiance map, specular from the
  // prefiltered environment cubemap. Specular IBL uses the split-sum BRDF LUT
  // (LO): specular = prefiltered * (F * brdf.x + brdf.y).
  vec3 R          = reflect(-V, N);
  vec3 F_ibl      = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
  vec3 kD_ibl     = (1.0 - F_ibl) * (1.0 - metallic);
  vec3 irradiance = texture(irradiance_map, N).rgb;
  vec3 prefiltered = textureLod(prefiltered_map, R, roughness * max_prefilter_mip).rgb;
  vec2 env_brdf    = texture(brdf_lut, vec2(max(dot(N, V), 0.0), roughness)).rg;
  vec3 spec_ibl    = prefiltered * (F_ibl * env_brdf.x + env_brdf.y) * specular_intensity *
                     float(u_ibl_specular);
  float ssao = ssao_enabled == 1 ? texture(ssao_map, gl_FragCoord.xy / viewport_size).r : 1.0;
  vec3 ambient = (kD_ibl * albedo * irradiance + spec_ibl) * ao * ibl_intensity * ssao;

  vec3 color = ambient + direct;
  for (int i = 0; i < point_light_count && i < MAX_POINT_LIGHTS; ++i) {
    vec3 contribution = PointLightContribution(point_light_positions[i], point_light_colors[i],
                                               point_light_intensities[i], point_light_radii[i],
                                               point_light_lo_attenuation[i] == 1, point_light_constants[i],
                                               point_light_linears[i], point_light_quadratics[i], N, V, albedo,
                                               metallic, roughness, specular_intensity, F0);
    if (point_light_has_shadow[i] == 1) {
      vec3 L_pl = normalize(point_light_positions[i] - FragPos);
      contribution *= PointShadowCalculation(i, point_light_positions[i], N, L_pl);
    }
    color += contribution;
  }
  for (int i = 0; i < spot_light_count && i < MAX_SPOT_LIGHTS; ++i) {
    color += SpotLightContribution(spot_light_positions[i], spot_light_directions[i], spot_light_colors[i],
                                   spot_light_intensities[i], spot_light_ranges[i], spot_light_cutoffs[i],
                                   spot_light_outer_cutoffs[i], spot_light_lo_attenuation[i] == 1,
                                   spot_light_constants[i], spot_light_linears[i], spot_light_quadratics[i],
                                   N, V, albedo, metallic, roughness, specular_intensity, F0);
  }
  // HDR linear output; tone mapping + gamma happen in the post-process pass.
  // Alpha is the material opacity so translucent surfaces (alpha blending on,
  // depth write off) composite over the opaque scene.
  FragColor = vec4(color, base_color_factor.a);
}
