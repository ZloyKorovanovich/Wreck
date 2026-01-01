#include "layout.hlsl"

#define PI 3.14159

struct Interpolators {
    float4 position_cs : SV_POSITION;
    float4 color : COLOR;
};

struct Targets {
    float4 color : SV_TARGET0;
    float depth : SV_DEPTH;
};

float3 rotate(float3 position, float angle) {
    return float3(
        position.x * cos(angle) - position.z * sin(angle),
        position.y,
        position.x * sin(angle) + position.z * cos(angle)
    );
}

Interpolators vertexMain(uint vertex_id : SV_VERTEXID) {
    float4 position = float4((rotate(0.05 * vertices[vertex_id].xyz, time_params.x * 1) + float3(0, 0, 0.5)), 1.0);

    Interpolators output = (Interpolators)0;
    output.position_cs = float4(position.xy / (position.z / 4.0 - 0.01) * screen_params.xy * screen_params.zw, position.z, 1.0);
    output.color = (1 - (float)(vertex_id / 3) / 12.0);

    return output;
}

Targets fragmentMain(Interpolators input) {
    Targets output = (Targets)0;
    output.color = input.color;
    output.depth = input.position_cs.z;
    return output;
}
