#include "layout.hlsl"

[numthreads(64, 1, 1)]
void computeMain(uint3 thread_id : SV_DISPATCHTHREADID) {
    float x = (float)(thread_id.x % 8) * 2.0 / 7.0 - 1.0;
    float y = (float)(thread_id.x / 8) * 2.0 / 7.0 - 1.0;
    positions[thread_id.x] = float4(x, y + sin((time_params.x + x) * 3) * 0.2, 0.5, 1.0);
}
