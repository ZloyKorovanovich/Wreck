#include "descriptors.hlsl"
#include "water_common.hlsl"
#include "skybox_common.hlsl"

/*
    [[vk::binding(5, 0)]] Texture2D           screen_color_sampled;
    [[vk::binding(6, 0)]] RWTexture2D<float4> screen_color_storage;
*/

static float2 quad_section[6] = {
    float2(0, 0),
    float2(0, 1),
    float2(1, 1),
    float2(0, 0),
    float2(1, 1),
    float2(1, 0)
};

#define SUBDIV_COUNT (32)

struct Interpolators {
    float4 position_cs : SV_Position;
    float4 position_ws : TEXCOORD0;
    float4 position_ss : TEXCOORD1;
    float4 surface     : TEXCOORD2;
};

Interpolators main_vertex(uint vertex_id : SV_VertexID) {
    uint quad_id = vertex_id / 6;
    uint vert_id = vertex_id % 6;
    uint x       = quad_id % SUBDIV_COUNT;
    uint y       = quad_id / SUBDIV_COUNT;

    float2 quad_pos = quad_section[vert_id];
    float2 pos      = float2(quad_pos.x + x, quad_pos.y + y) / SUBDIV_COUNT * 2 - 1;

    float4 pos_cs = float4(pos, 0, 1);
    float4 pos_ws = mul(global_buffer.camera_inv_vp, pos_cs);
    pos_ws = float4(pos_ws.xyz / pos_ws.w, 1);

    float2 target_pos = pos_ws.xz;
    float2 sample_pos = pos_ws.xz;
    float3 displ      = 0.0;

    for(uint i = 0; i != 6; i++) {
        displ = waves_displace(sample_pos, global_buffer.time.x, 32 * (i + 1) / 6.0);
        sample_pos = target_pos - displ.xz;
    }

    Interpolators output = (Interpolators)0;
    output.position_cs = pos_cs;
    output.position_ws = pos_ws;
    output.position_ss = pos_cs;
    output.surface     = float4(displ.y, 0, 0, 0);
    return output;
}

float3 compute_fog_color(float3 original, float dist, float visibility, float3 eye_dir, float cam_depth) {
    float3 dist_decay = float3(
        exp(dist * -0.17),
        max(pow(1 / max(dist, 1), 0.05), 0.3),
        1
    );
    float3 vis_decay = float3(
        exp(visibility * -0.17),
        max(pow(1 / max(visibility, 1), 0.05), 0.3),
        1
    );

    float3 fog_color  = float3(0.2, 0.68, 0.73) * 0.7;
    float  depth_term = 1 - min(cam_depth / 350, 0.89);
    float  top_grad   = saturate((eye_dir.y + 0.7) / 1.7);
    fog_color = lerp(fog_color, 1, pow(top_grad, 5) * depth_term);

    return lerp(original * dist_decay, fog_color * vis_decay, saturate(dist / max(visibility, 0.01))) * depth_term;
}

float4 main_fragment(Interpolators input) : SV_Target0 {
    float2 screen_uv    = input.position_cs.xy / global_buffer.screen_params.zw;
    float3 position_ws  = input.position_ws.xyz;

    float3 screen_color = color_copy_sampled.Sample(sampler_nearest_clamp, screen_uv, 0).rgb;

    float  water_mask   = 1 - smoothstep(0.003, 0.005, input.position_ws.y - input.surface.x);
    float  line_mask    = smoothstep(0.001, 0.003, input.position_ws.y - input.surface.x);

    float  depth        = screen_depth_sampled.Sample(sampler_nearest_clamp, screen_uv, 0).r;
    float4 pixel_pos_cs = float4(input.position_ss.x, input.position_ss.y, depth, 1);
    float4 pixel_pos_ws = mul(global_buffer.camera_inv_vp, pixel_pos_cs);
    pixel_pos_ws        = float4(pixel_pos_ws.xyz / pixel_pos_ws.w, 1);
    float  dist         = length(pixel_pos_ws.xyz - global_buffer.camera_position.xyz);

    float3 light_dir = global_buffer.sun_direction.xyz;
    float3 eye_dir   = normalize(pixel_pos_ws.xyz - global_buffer.camera_position.xyz);

    float3 hor_view  = normalize(float3(eye_dir.x, light_dir.y * light_dir.y, eye_dir.y));
    float3 sky_color = saturate(sky(hor_view, light_dir));

    float3 color = compute_fog_color(screen_color, dist, 18.0, eye_dir, max(-global_buffer.camera_position.y, 0));
    color = lerp(color, sqrt(sky_color * float3(0.2, 0.68, 0.73)), line_mask);

    return float4(lerp(screen_color, color, water_mask), 1);
}

