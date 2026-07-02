
float3 sky(float3 dir, float3 sun_dir) {
    float3 sky_color       = float3(0.65, 0.8, 0.9);
    float3 sky_night_color = float3(0.2, 0.3, 0.4);
    float3 horizon_color   = float3(1.0, 0.7, 0.67);

    float ground   = 1 - smoothstep(-0.01, 0.00, dir.y);
    float horizon  = saturate(pow(1 - saturate(dir.y), 2.0));
    float sun      = 1 - pow(1 - saturate(dot(dir, sun_dir)), 0.1);
    float glow     = pow((horizon + sun * 1.3) * 1.3, 2) * saturate(dot(sun_dir.xz, dir.xz) * 0.5 + 0.7);
    
    float3 color = 0;
    color = lerp(sky_night_color, sky_color, sun_dir.y * 0.5 + 0.5);
    color = lerp(color, float3(0.89, 0.84, 0.78), horizon * sqrt(saturate(sun_dir.y)));
    color = lerp(color, horizon_color, glow * saturate(pow(1 - sun_dir.y, 3.0)));

    return color;
}

float3 sun(float3 dir, float3 sun_dir) {
    return pow(max(0.0, dot(dir, sun_dir)), 1248.0) * 100.0 * float3(1.0, 0.8, 0.4);;
}
