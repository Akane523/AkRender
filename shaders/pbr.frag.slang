// pbr.frag.slang
// Cook-Torrance PBR fragment shader.
//
// Implements:
//   - GGX / Trowbridge-Reitz normal distribution function
//   - Smith's geometry shadowing/masking with Schlick-GGX
//   - Schlick Fresnel approximation
//   - Reinhard HDR tonemapping + gamma correction

import "pbr_brdf";   // shared BRDF functions (see pbr_brdf.slang)

struct FragInput {
    float4 clipPos  : SV_Position;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

cbuffer SceneUniforms : register(b0) {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float3   cameraPos;
    float    _pad0;
};

cbuffer LightUniforms : register(b1) {
    float3 lightPos;
    float  _pad1;
    float3 lightColor;
    float  _pad2;
};

cbuffer MaterialUniforms : register(b2) {
    float3 baseColor;
    float  metallic;
    float  roughness;
    float  ao;
    float2 _pad3;
};

static const float PI = 3.14159265358979323846f;

[shader("fragment")]
float4 fragMain(FragInput input) : SV_Target {
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldPos);

    // Fresnel reflectance at normal incidence
    float3 F0 = lerp(float3(0.04f), baseColor, metallic);

    // Single point-light contribution
    float3 L        = normalize(lightPos - input.worldPos);
    float3 H        = normalize(V + L);
    float  dist     = length(lightPos - input.worldPos);
    float  atten    = 1.0f / (dist * dist);
    float3 radiance = lightColor * atten;

    float  NDF  = distributionGGX(N, H, roughness);
    float  G    = geometrySmith(N, V, L, roughness);
    float3 F    = fresnelSchlick(max(dot(H, V), 0.0f), F0);

    float3 numerator   = NDF * G * F;
    float  denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 1e-4f;
    float3 specular    = numerator / denominator;

    float3 kD  = (float3(1.0f) - F) * (1.0f - metallic);
    float  NdotL = max(dot(N, L), 0.0f);

    float3 Lo = (kD * baseColor / PI + specular) * radiance * NdotL;

    // Ambient (IBL placeholder)
    float3 ambient = float3(0.03f) * baseColor * ao;
    float3 color   = ambient + Lo;

    // Reinhard HDR tonemapping
    color = color / (color + float3(1.0f));
    // Gamma correction (linear → sRGB)
    color = pow(max(color, float3(0.0f)), float3(1.0f / 2.2f));

    return float4(color, 1.0f);
}
