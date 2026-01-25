
struct Attributes{
    [[vk::location(0)]] [[vk::offset(0)]]  float4 position : POSITION;
    [[vk::location(1)]] [[vk::offset(16)]] float4 normal : NORMAL;
    [[vk::location(2)]] [[vk::offset(32)]] float4 uv : TEXCOORD0;
};

float4 vertexMain(Attributes attributes) : SV_POSITION {
    return float4(attributes.position.x, -attributes.position.y, 0.5, 1.0);
}

struct Pixel {
    float4 color : SV_TARGET;
    float depth : SV_DEPTH;
};

Pixel fragmentMain(float4 position : SV_POSITION) {
    Pixel output = (Pixel)0;
    output.color = float4(1.0, 1.0, 0.0, 1.0);
    output.depth = 0.5;
    return output;
}
