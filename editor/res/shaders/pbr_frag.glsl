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

uniform vec4  base_color_factor = vec4(1.0);
uniform float metallic_factor   = 1.0;
uniform float roughness_factor  = 1.0;

uniform vec3 view_pos;
uniform vec3 light_dir   = normalize(vec3(-0.3, -1.0, -0.4));
uniform vec3 light_color = vec3(2.5);
uniform float exposure   = 1.2;

uniform sampler2D shadow_map;
uniform mat4      light_view_proj;

#define MAX_POINT_LIGHTS 8
uniform int   point_light_count = 0;
uniform vec3  point_light_positions[MAX_POINT_LIGHTS];
uniform vec3  point_light_colors[MAX_POINT_LIGHTS];
uniform float point_light_intensities[MAX_POINT_LIGHTS];
uniform float point_light_radii[MAX_POINT_LIGHTS];

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

// Cheap procedural environment (a vertical studio-like gradient). Real IBL via
// an HDR cubemap should replace this later, but this already gives metals
// something to reflect so they stop looking flat/dark.
vec3 EnvironmentColor(vec3 dir) {
  vec3  zenith  = vec3(0.55, 0.60, 0.68);
  vec3  horizon = vec3(0.30, 0.30, 0.32);
  vec3  nadir   = vec3(0.10, 0.10, 0.11);
  float t       = clamp(dir.y, -1.0, 1.0) * 0.5 + 0.5;
  return (t < 0.5) ? mix(nadir, horizon, t * 2.0) : mix(horizon, zenith, (t - 0.5) * 2.0);
}

float ShadowCalculation(vec3 frag_pos_world, vec3 N, vec3 L) {
  vec4 clip = light_view_proj * vec4(frag_pos_world, 1.0);
  vec3 proj = clip.xyz / clip.w;
  proj      = proj * 0.5 + 0.5;
  if (proj.z > 1.0) {
    return 1.0;
  }
  float closest = texture(shadow_map, proj.xy).r;
  float current = proj.z;
  float bias    = max(0.002 * (1.0 - dot(N, L)), 0.0005);
  return (current - bias > closest) ? 0.0 : 1.0;
}

vec3 PointLightContribution(vec3 light_pos, vec3 light_color, float intensity, float radius, vec3 N, vec3 V,
                            vec3 albedo, float metallic, float roughness, vec3 F0) {
  vec3  L        = light_pos - FragPos;
  float distance = length(L);
  L             = normalize(L);

  float attenuation = clamp(1.0 - pow(distance / radius, 4.0), 0.0, 1.0);
  attenuation *= attenuation;
  attenuation /= max(distance * distance, 0.001);

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
  return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
  vec3 albedo = has_albedo_map == 1 ? texture(albedo_map, TexCoord).rgb : vec3(1.0);
  albedo *= base_color_factor.rgb;

  float metallic  = metallic_factor;
  float roughness = roughness_factor;
  if (has_metallic_roughness_map == 1) {
    vec3 mr  = texture(metallic_roughness_map, TexCoord).rgb;
    roughness *= mr.g;
    metallic  *= mr.b;
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
  vec3  direct = (kD * albedo / PI + specular) * light_color * NdotL;
  direct *= ShadowCalculation(FragPos, N, L);

  // Image-based ambient approximation: diffuse irradiance from the normal,
  // specular environment from the (roughness-blurred) reflection direction.
  vec3 R      = normalize(reflect(-V, N));
  R           = normalize(mix(R, N, roughness));
  vec3 F_ibl  = FresnelSchlick(max(dot(N, V), 0.0), F0);
  vec3 kD_ibl = (1.0 - F_ibl) * (1.0 - metallic);
  vec3 ambient = (kD_ibl * albedo * EnvironmentColor(N) + F_ibl * EnvironmentColor(R)) * ao;

  vec3 color = ambient + direct;
  for (int i = 0; i < point_light_count && i < MAX_POINT_LIGHTS; ++i) {
    color += PointLightContribution(point_light_positions[i], point_light_colors[i], point_light_intensities[i],
                                    point_light_radii[i], N, V, albedo, metallic, roughness, F0);
  }
  color *= exposure;

  color     = color / (color + vec3(1.0));
  color     = pow(color, vec3(1.0 / 2.2));

  FragColor = vec4(color, 1.0);
}
