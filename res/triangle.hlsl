static float2 positions[6] = {
    float2(-0.2,-0.5),
    float2(-0.2, 0.5),
    float2( 0.2, 0.9),

    float2(-0.2,-0.5),
    float2( 0.2, 0.9),
    float2( 0.2,-0.9)
};

struct Interpolators {
    float4 position_cs : SV_POSITION;
    float4 position_raw : TEXCOORD0;
};

struct Pixel {
    float4 color : SV_TARGET;
    float depth : SV_DEPTH;
};

Interpolators vertexMain(uint vertex_id : SV_VERTEXID) {
    Interpolators output = (Interpolators)0;
    output.position_raw = float4(positions[vertex_id].x, -positions[vertex_id].y, 0.0, 1);
    output.position_cs = output.position_raw;
    return output;
}

Pixel fragmentMain(Interpolators input) {
    Pixel output = (Pixel)0;
    output.color = lerp(float4(0.05, 0.9, 0.1, 1.0), float4(0.9, 0.05, 0.02, 1.0), smoothstep(0.10, 0.15, abs(input.position_raw.x)));
    output.depth = 0.5;
    return output;
}