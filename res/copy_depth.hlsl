#include "descriptors.hlsl"

static float2 full_screen_quad[6] = {
    float2(-1,-1),
    float2(-1, 1),
    float2( 1, 1),
    float2(-1,-1),
    float2( 1, 1),
    float2( 1,-1)
};

struct Interpolators {
    float4 position_cs : SV_Position;
    float4 position_uv : TEXCOORD0;
};

Interpolators main_vertex(uint vertex_id : SV_VertexID) {
    Interpolators output = (Interpolators)0;
    output.position_cs = float4( full_screen_quad[vertex_id], 0, 1);
    output.position_uv = float4((full_screen_quad[vertex_id] + 1) * 0.5 * global_buffer.screen_params.xy / global_buffer.screen_params.zw, 0, 0);
    return output;
}

float4 main_fragment(Interpolators input) : SV_Target0 {
    return screen_depth_sampled.Sample(sampler_nearest_clamp, input.position_uv.xy, 0);
}
