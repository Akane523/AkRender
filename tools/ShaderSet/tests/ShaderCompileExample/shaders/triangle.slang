// triangle.slang — vertex/fragment shader using math_utils module
import math_utils;

[shader("vertex")]
V2F vsMain(VSInput input) {
    V2F output;
    output.position = make_vertex(input.position);
    output.color    = input.color;
    return output;
}

[shader("fragment")]
float4 fsMain(V2F input) : SV_TARGET {
    return float4(input.color, 1.0);
}
