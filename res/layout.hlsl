[[vk::binding(0, 0)]] cbuffer uniform_buffer {
    float4 screen_params;
    float4 time_params;
}
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> positions;

[[vk::binding(1, 1)]] StructuredBuffer<float4> vertices;
