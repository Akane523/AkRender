// triangle.vert.slang
// Basic pass-through vertex shader used by the triangle example.

struct VertexInput {
    float3 position : POSITION;
    float3 color    : COLOR;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 color    : COLOR;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput input) {
    VertexOutput output;
    output.position = float4(input.position, 1.0f);
    output.color    = input.color;
    return output;
}
