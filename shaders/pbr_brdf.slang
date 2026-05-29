// pbr_brdf.slang
// Shared PBR BRDF functions used by pbr.frag.slang.
// This file is a Slang *module* – it is imported, not compiled standalone.

/// GGX/Trowbridge-Reitz normal distribution function.
float distributionGGX(float3 N, float3 H, float roughness) {
    float a     = roughness * roughness;
    float a2    = a * a;
    float NdotH = max(dot(N, H), 0.0f);

    float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    denom       = 3.14159265f * denom * denom;

    return a2 / max(denom, 1e-7f);
}

/// Schlick-GGX geometry term (single direction).
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

/// Smith's geometry function (both light and view directions).
float geometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    return geometrySchlickGGX(NdotV, roughness)
         * geometrySchlickGGX(NdotL, roughness);
}

/// Schlick Fresnel approximation.
float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (float3(1.0f) - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}
