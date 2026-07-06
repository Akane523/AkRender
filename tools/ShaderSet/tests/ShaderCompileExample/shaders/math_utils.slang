// math_utils.slang — reusable Slang module
export struct VSInput {
    float2 position;
    float3 color;
};

export struct V2F {
    float4 position : SV_POSITION;
    float3 color    : COLOR;
};

export float4 make_vertex(float2 pos) {
    return float4(pos, 0.0, 1.0);
}
