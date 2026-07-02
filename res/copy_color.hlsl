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

static const float bayer8x8[64] = {
    0, 32, 8, 40, 2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44, 4, 36, 14, 46, 6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
    3, 35, 11, 43, 1, 33, 9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47, 7, 39, 13, 45, 5, 37,
    63, 31, 55, 23, 61, 29, 53, 21
};

float ordered_dither(float value, float2 pixel_pos, float intensity = 1.0) {
    uint2 pos = uint2(pixel_pos) % 8;
    float threshold = bayer8x8[pos.y * 8 + pos.x] / 64.0;
    return value + (threshold - 0.5) * intensity / 255.0;
}

Interpolators main_vertex(uint vertex_id : SV_VertexID) {
    Interpolators output = (Interpolators)0;
    output.position_cs = float4( full_screen_quad[vertex_id], 0, 1);
    output.position_uv = float4((full_screen_quad[vertex_id] + 1) * 0.5 * global_buffer.screen_params.xy / global_buffer.screen_params.zw, 0, 0);
    return output;
}

float4 main_fragment(Interpolators input) : SV_Target0 {
    float3 color = screen_color_sampled.Sample(sampler_nearest_clamp, input.position_uv.xy, 0).rgb;
    float  dither_amount = 1.0; // Adjust for strength
    color.r = ordered_dither(color.r, input.position_uv.xy * global_buffer.screen_params.xy, dither_amount);
    color.g = ordered_dither(color.g, input.position_uv.xy * global_buffer.screen_params.xy, dither_amount);
    color.b = ordered_dither(color.b, input.position_uv.xy * global_buffer.screen_params.xy, dither_amount);

    return float4(color, 1);
}
