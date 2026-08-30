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
uniform vec3 light_color = vec3(1.0);
uniform vec3 ambient     = vec3(0.03);

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
  vec3  amb    = ambient * albedo * ao;

  vec3 color = amb + direct;

  color     = color / (color + vec3(1.0));
  color     = pow(color, vec3(1.0 / 2.2));

  FragColor = vec4(color, 1.0);
}
