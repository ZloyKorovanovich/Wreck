

struct GlobalBuffer {
    /* [screen_width, screen_height, buffer_width, buffer_height] */
    float4   screen_params;
    float4   sun_direction;
    float4   camera_position;
    float4   time;
    float4x4 camera_vp;
    float4x4 camera_inv_vp;
    float4x4 camera_inv_v;
};

/* SET 0 */
[[vk::binding(0, 0)]] cbuffer             global_cbuffer {GlobalBuffer global_buffer; };
[[vk::binding(1, 0)]] SamplerState        sampler_linear_repeat;
[[vk::binding(2, 0)]] SamplerState        sampler_linear_clamp;
[[vk::binding(3, 0)]] SamplerState        sampler_nearest_repeat;
[[vk::binding(4, 0)]] SamplerState        sampler_nearest_clamp;
[[vk::binding(5, 0)]] Texture2D           screen_color_sampled;
[[vk::binding(6, 0)]] RWTexture2D<float4> screen_color_storage;
[[vk::binding(7, 0)]] Texture2D           screen_depth_sampled;
[[vk::binding(8, 0)]] Texture2D           color_copy_sampled;
[[vk::binding(9, 0)]] Texture2D           depth_copy_sampled;
