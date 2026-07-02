#include "descriptors.hlsl"
#include "skybox_common.hlsl"

static float2 full_screen_quad[6] = {
    float2(-1,-1),
    float2(-1, 1),
    float2( 1, 1),
    float2(-1,-1),
    float2( 1, 1),
    float2( 1,-1)
};

struct Interpolators {
    float4 position_cs  : SV_Position;
    float4 direction_ws : TEXCOORD0;
};

struct Pixel {
    float4 color : SV_TARGET0;
    float  depth : SV_DEPTH;
};

Interpolators main_vertex(uint vertex_id : SV_VertexID) {
    float2 quad_position   = full_screen_quad[vertex_id];
    float4 pixel_near_plane = mul(global_buffer.camera_inv_vp, float4(quad_position.x, quad_position.y, 0.0, 1.0));
    float3 pixel_direction = normalize(pixel_near_plane.xyz / pixel_near_plane.w - global_buffer.camera_position.xyz);

    Interpolators output = (Interpolators)0;
    output.position_cs   = float4(quad_position, 0.0, 1.0);
    output.direction_ws  = float4(pixel_direction, 0.0);
    return output;
}

Pixel main_fragment(Interpolators input) {
    float3 direction_ws = normalize(input.direction_ws.xyz);
    float3 sky_color = sky(direction_ws, global_buffer.sun_direction.xyz);

    Pixel pixel = (Pixel)0;
    pixel.color = float4(saturate(sky_color + sun(direction_ws, global_buffer.sun_direction.xyz)), 1.0);
    pixel.depth = 1.0;
    return pixel;
}
