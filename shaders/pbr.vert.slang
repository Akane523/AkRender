// pbr.vert.slang
// PBR vertex shader – transforms geometry and forwards world-space attributes.

struct PBRVertexInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct PBRVertexOutput {
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

[shader("vertex")]
PBRVertexOutput vertMain(PBRVertexInput input) {
    float4 worldPos4  = mul(model, float4(input.position, 1.0f));

    PBRVertexOutput output;
    output.worldPos = worldPos4.xyz;
    output.normal   = normalize(mul((float3x3)model, input.normal));
    output.uv       = input.uv;
    output.clipPos  = mul(proj, mul(view, worldPos4));
    return output;
}
