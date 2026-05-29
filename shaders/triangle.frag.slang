// triangle.frag.slang
// Basic fragment shader used by the triangle example.

struct FragInput {
    float4 position : SV_Position;
    float3 color    : COLOR;
};

[shader("fragment")]
float4 fragMain(FragInput input) : SV_Target {
    return float4(input.color, 1.0f);
}
