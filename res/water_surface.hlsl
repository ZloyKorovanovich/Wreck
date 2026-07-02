#include "descriptors.hlsl"
#include "water_common.hlsl"
#include "skybox_common.hlsl"

#define PI (3.14159265358979)

static float2 quad[6] = {
    float2(0, 0),
    float2(0, 1),
    float2(1, 1),
    float2(0, 0),
    float2(1, 1),
    float2(1, 0)
};

static float2 blocks[8] = {
    float2(-1, -1),
    float2(-1,  0),
    float2(-1,  1),
    float2( 0, -1),
    float2( 0,  1),
    float2( 1, -1),
    float2( 1,  0),
    float2( 1,  1)
};

#define LOD_0_RES (600)
#define LOD_N_RES (1)
#define LOD_SIZE  (46)

#define NORMAL_I_COUNT (5)
const static uint normal_lod_iterations[NORMAL_I_COUNT] = {32, 22, 14, 6, 0};

const static float three_pow[8] = {0, 1, 3, 9, 27, 81, 243, 729};

float4 surface_position(uint vertex_id) {
    uint quad_id  = vertex_id / 6;
    uint vert_id  = vertex_id % 6;

    float2 lpos = quad[vert_id];
    float2 gpos = 0.0;
    float2 pos  = 0.0;

    if(quad_id < LOD_0_RES * LOD_0_RES) {
        uint x = quad_id / LOD_0_RES;
        uint y = quad_id % LOD_0_RES;

        gpos = float2(x, y);
        pos  = ((gpos + lpos) / (LOD_0_RES) - 0.5) * LOD_SIZE;
    } else {
        quad_id    = quad_id - LOD_0_RES * LOD_0_RES;
        uint lod_n = quad_id / (LOD_N_RES * LOD_N_RES * 8);

        quad_id       = quad_id - (LOD_N_RES * LOD_N_RES * 8) * lod_n; 
        uint block_id = quad_id / (LOD_N_RES * LOD_N_RES);
        quad_id       = quad_id % (LOD_N_RES * LOD_N_RES);

        uint x       = quad_id / LOD_N_RES;
        uint y       = quad_id % LOD_N_RES;
        float2 block = blocks[block_id];

        gpos = float2(x, y);
        pos  = (((gpos + lpos) / LOD_N_RES - 0.5) + block) * LOD_SIZE * three_pow[lod_n];
    }

    return float4(pos, lpos);
}

struct Interpolators {
    float4 position_cs : SV_Position;
    float4 position_ws : TEXCOORD0;
    float4 uv_ws       : TEXCOORD1;
};

struct Pixel {
    float4 color : SV_TARGET0;
    float  depth : SV_DEPTH;
};

Interpolators main_vertex(uint vertex_id : SV_VertexID) {
    float4 position = surface_position(vertex_id) + float4(global_buffer.camera_position.xz, 0.0, 0.0);

    float3 view_vector = global_buffer.camera_position.xyz - float3(position.x, 0, position.y);
    float  amplitude   = 1 - saturate((length(view_vector) - 5) / 17.0);
    float3 displace    = waves_displace(position.xy, global_buffer.time.x, amplitude * 32) * amplitude;
    float4 position_ws = float4(float3(position.x, 0, position.y) + displace, 1);

    Interpolators output = (Interpolators)0;
    output.position_cs = mul(global_buffer.camera_vp, position_ws);
    output.position_ws = position_ws;
    output.uv_ws       = position;
    return output;
}

Pixel main_fragment(Interpolators input, bool is_front_face : SV_IsFrontFace) {
    float2 screen_uv = input.position_cs.xy / global_buffer.screen_params.zw;

    float3 view_vector   = global_buffer.camera_position.xyz - input.position_ws.xyz;
    float3 view_dir      = normalize(view_vector);
    float3 light_dir     = global_buffer.sun_direction.xyz;
    
    // definitely can be improved 
    float dist      = length(view_vector);
    float inv_abs_h = 1.0 / max(1, sqrt(abs(view_vector.y)));
    
    float normal_decay      = 1 - saturate(dist * inv_abs_h / 20.0);
    float normal_lod        = saturate(log(dist * 0.19) * 0.3) * inv_abs_h * NORMAL_I_COUNT;
    uint  normal_lod_min    = normal_lod_iterations[floor(normal_lod)];
    uint  normal_lod_max    = normal_lod_iterations[ceil(normal_lod)];
    uint  normal_iterations = (uint)lerp(normal_lod_min, normal_lod_max, frac(normal_lod));

    float4 packed_normal = waves_normals(input.uv_ws.xy, global_buffer.time.x, normal_iterations);
    float3 normal        = float3(
        packed_normal.x * normal_decay, 
        lerp(1, packed_normal.y, normal_decay), 
        packed_normal.z * normal_decay
    );
    float  height        = packed_normal.w;

    float3 reflect_dir = reflect(-view_dir, normal);
    reflect_dir.y      = abs(reflect_dir.y);

    float3 sun_color   = saturate(sun(light_dir, light_dir));
    float3 water_color = float3(0.2, 0.68, 0.73) * 0.7;
    float3 deep_color  = water_color * water_color;
    
    float3 color = 0;

    if(is_front_face) {
        float3 sss_color = (water_color + sun_color * float3(0.5, 0.8, 0.7)) / 2;
        float3 sky_color = saturate(sky(reflect_dir, light_dir));

        float fresnel  = 1.0 - saturate(dot(view_dir, normal));
        float sun_alig = saturate(dot(-view_dir.xz, light_dir.xz));

        float sky_mask = pow(fresnel, 3.0);
        float sun_mask = smoothstep(0.9, 0.95, pow(saturate(dot(reflect_dir, light_dir)), 9.13));
        float sss_mask = max(sun_alig * sun_alig, light_dir.y * 0.8) * pow(height, 1.4);

        color = lerp(deep_color, water_color, pow(height, 2.4));
        color = lerp(color, sss_color, sss_mask);
        color = lerp(color, sky_color, sky_mask);
        color = lerp(color, sun_color, sun_mask);
    } else {
        normal = -normal;

        float3 hor_view = normalize(float3(-view_dir.x, light_dir.y * light_dir.y, -view_dir.y));

        float3 scene_color   = sqrt(color_copy_sampled.Sample(sampler_linear_clamp, saturate(screen_uv + normal.xz * 0.01), 0).rgb);
        float3 sky_color     = saturate(sky(hor_view, light_dir));
        float3 horizon_color = water_color;

        float fresnel            = pow(1 - saturate(dot(view_dir, normal)), 4.0);
        float window             = dot(view_dir, normal);
        float bright_window      = smoothstep(0.72, 0.74, window);
        float transparent_window = smoothstep(0.78, 0.83, window);
        float sun_mask           = pow(saturate(dot(-view_dir, light_dir)), 10.4);

        color = horizon_color;
        color = lerp(color, sky_color, bright_window);
        color = lerp(color, sun_color, bright_window * sun_mask);
        color = lerp(color, scene_color, transparent_window);
    }

    Pixel pixel = (Pixel)0;
    pixel.color = float4(color, 1);
    pixel.depth = input.position_cs.z;
    return pixel;
}
