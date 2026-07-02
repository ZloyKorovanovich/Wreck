/*
#define PI (3.14159265358979)

float3 ocean_wave_displace(float2 position, float2 direction, float steepness, float length, float time) {
    float k = 2 * PI / length;
    float c = sqrt(9.8 / k);
    float f = k * (dot(direction, position) - c * time);
    float a = steepness / k;

    return float3(
        a * cos(f) * direction.x,
        a * sin(f),
        a * cos(f) * direction.y
    );
}

float2x3 ocean_wave_normal(float2 position, float2 direction, float steepness, float length, float time) {
    float k = 2 * PI / length;
    float c = sqrt(9.8 / k);
    float f = k * (dot(direction, position) - c * time);

    return float2x3(
        float3(
            steepness * sin(f) * direction.y * -direction.x,
            steepness * cos(f) * direction.y,
            steepness * sin(f) * direction.y * -direction.y
        ),
        float3(
            steepness * sin(f) * direction.x * -direction.x,
            steepness * cos(f) * direction.x,
            steepness * sin(f) * direction.y * -direction.x
        )
    );
}

#define OCEAN_WAVES_D (32)
#define OCEAN_WAVES_N (32)
#define SPREAD      (0.8)
#define MIN_LENGTH  (0.2)
#define MAX_LENGTH  (3.0)
#define MIN_STEEPNESS (0.01)
#define MAX_STEEPNESS (0.07)

float rand(float n) {
    return frac(sin(n * 12.9898f) * 43758.5453f);
}

static const float4 sea_waves[32] = {
    // BIG SWELL
    float4(-0.10f, 3.20f, 0.040f, 1.55f),
    float4( 0.06f, 2.90f, 0.046f, 1.48f),
    float4(-0.16f, 2.60f, 0.042f, 1.40f),
    float4( 0.11f, 2.30f, 0.050f, 1.34f),
    float4(-0.05f, 2.05f, 0.044f, 1.28f),
    float4( 0.18f, 1.82f, 0.048f, 1.22f),

    // MEDIUM WAVES
    float4(-0.26f, 1.65f, 0.052f, 1.28f),
    float4( 0.13f, 1.50f, 0.056f, 1.24f),
    float4(-0.31f, 1.35f, 0.050f, 1.20f),
    float4( 0.28f, 1.22f, 0.060f, 1.17f),
    float4(-0.17f, 1.10f, 0.057f, 1.14f),
    float4( 0.36f, 1.00f, 0.054f, 1.10f),
    float4(-0.40f, 0.90f, 0.062f, 1.08f),
    float4( 0.08f, 0.82f, 0.058f, 1.05f),

    // SMALL WAVES
    float4(-0.46f, 0.72f, 0.070f, 1.18f),
    float4( 0.38f, 0.64f, 0.076f, 1.24f),
    float4(-0.22f, 0.58f, 0.068f, 1.20f),
    float4( 0.53f, 0.52f, 0.074f, 1.28f),
    float4(-0.57f, 0.47f, 0.064f, 1.24f),
    float4( 0.25f, 0.42f, 0.070f, 1.32f),
    float4(-0.35f, 0.38f, 0.060f, 1.28f),
    float4( 0.64f, 0.34f, 0.062f, 1.36f),

    // FINE CHOP
    float4(-0.76f, 0.30f, 0.060f, 1.42f),
    float4( 0.42f, 0.28f, 0.064f, 1.48f),
    float4(-0.28f, 0.25f, 0.058f, 1.45f),
    float4( 0.84f, 0.23f, 0.056f, 1.52f),
    float4(-0.64f, 0.21f, 0.054f, 1.49f),
    float4( 0.12f, 0.19f, 0.052f, 1.56f),
    float4(-0.90f, 0.17f, 0.049f, 1.53f),
    float4( 0.57f, 0.16f, 0.047f, 1.60f),
    float4(-0.46f, 0.15f, 0.045f, 1.57f),
    float4( 0.96f, 0.14f, 0.043f, 1.64f)
};

float3 waves_displace(float2 position, float time) {
    float3 displace = 0;

    [unroll(OCEAN_WAVES_D)]
    for(uint i = 0; i != OCEAN_WAVES_D; i++) {
        float  angle     = sea_waves[i].x;
        float  length    = sea_waves[i].y;
        float  steepness = sea_waves[i].z;
        float  speed     = sea_waves[i].w;
        float2 direction = float2(sin(angle), cos(angle));
        displace += ocean_wave_displace(position, direction, steepness, length, time * speed);
    }

    return displace;
}

float3 waves_normals(float2 position, float time) {
    float2x3 binormal_tangent = float2x3(
        float3(0.0f, 0.0f, 1.0f),
        float3(1.0f, 0.0f, 0.0f)
    );

    [unroll(OCEAN_WAVES_N)]
    for(uint i = 0; i != OCEAN_WAVES_N; i++) {
        float  angle     = sea_waves[i].x;
        float  length    = sea_waves[i].y;
        float  steepness = sea_waves[i].z;
        float  speed     = sea_waves[i].w;
        float2 direction = float2(sin(angle), cos(angle));
        binormal_tangent += ocean_wave_normal(position, direction, steepness, length, time * speed);
    }

    return normalize(cross(binormal_tangent[0], binormal_tangent[1]));
}
*/

#define OCEAN_MAX_WAVES_D 56
#define OCEAN_MAX_WAVES_N 56

struct OceanWave {
    // x = kx
    // y = kz
    // z = omega
    // w = vertical amplitude
    float4 phase_data;

    // x = horizontal X amplitude
    // y = horizontal Z amplitude
    // z = phase offset
    // w = unused
    float4 displacement_data;
};

static const OceanWave sea_waves[32] = {
    // Big swell
    { float4(-0.1960225f,  1.9536861f,  6.7992292f, 0.0203718f), float4(-0.0020338f,  0.0202701f, 0.0f, 0.0f) },
    { float4( 0.1299190f,  2.1627169f,  6.8197066f, 0.0212313f), float4( 0.0012731f,  0.0211931f, 0.0f, 0.0f) },
    { float4(-0.3850099f,  2.3857431f,  6.8130933f, 0.0173797f), float4(-0.0027689f,  0.0171577f, 0.0f, 0.0f) },
    { float4( 0.2998945f,  2.7153088f,  6.9333616f, 0.0183028f), float4( 0.0020093f,  0.0181922f, 0.0f, 0.0f) },
    { float4(-0.1531846f,  3.0611380f,  7.0151346f, 0.0143558f), float4(-0.0007175f,  0.0143378f, 0.0f, 0.0f) },
    { float4( 0.6180637f,  3.3965232f,  7.0962206f, 0.0139038f), float4( 0.0024892f,  0.0136791f, 0.0f, 0.0f) },

    // Medium waves
    { float4(-0.9789605f,  3.6800044f,  7.8193557f, 0.0136555f), float4(-0.0035106f,  0.0131965f, 0.0f, 0.0f) },
    { float4( 0.5430102f,  4.1534447f,  7.9447279f, 0.0133690f), float4( 0.0017331f,  0.0132562f, 0.0f, 0.0f) },
    { float4(-1.4198074f,  4.4323617f,  8.1043341f, 0.0107430f), float4(-0.0032772f,  0.0102309f, 0.0f, 0.0f) },
    { float4( 1.4232736f,  4.9495815f,  8.3120648f, 0.0116501f), float4( 0.0032196f,  0.0111964f, 0.0f, 0.0f) },
    { float4(-0.9663673f,  5.6296470f,  8.5292625f, 0.0099790f), float4(-0.0016883f,  0.0098352f, 0.0f, 0.0f) },
    { float4( 2.2134043f,  5.8804132f,  8.6316865f, 0.0085944f), float4( 0.0030276f,  0.0080434f, 0.0f, 0.0f) },
    { float4(-2.7186529f,  6.4302188f,  8.9331674f, 0.0088808f), float4(-0.0034584f,  0.0081798f, 0.0f, 0.0f) },
    { float4( 0.6123400f,  7.6379144f,  9.0988257f, 0.0075694f), float4( 0.0006049f,  0.0075452f, 0.0f, 0.0f) },


    // Small waves
    { float4(-3.8741781f,  7.8195332f, 10.9123612f, 0.0080214f), float4(-0.0035611f,  0.0071876f, 0.0f, 0.0f) },
    { float4( 3.6415032f,  9.1171437f, 12.1628309f, 0.0077413f), float4( 0.0028714f,  0.0071891f, 0.0f, 0.0f) },
    { float4(-2.3640986f, 10.5719733f, 12.3643196f, 0.0062771f), float4(-0.0013698f,  0.0061258f, 0.0f, 0.0f) },
    { float4( 6.1083840f, 10.4253398f, 13.9287220f, 0.0061243f), float4( 0.0030960f,  0.0052841f, 0.0f, 0.0f) },
    { float4(-7.2140599f, 11.2549358f, 14.1930499f, 0.0047874f), float4(-0.0025834f,  0.0040305f, 0.0f, 0.0f) },
    { float4( 3.7011546f, 14.4948959f, 15.9827777f, 0.0046792f), float4( 0.0011576f,  0.0045337f, 0.0f, 0.0f) },
    { float4(-5.6697118f, 15.5322443f, 16.2937536f, 0.0036287f), float4(-0.0012443f,  0.0034087f, 0.0f, 0.0f) },
    { float4(11.0361459f, 14.8226949f, 18.3021631f, 0.0033550f), float4( 0.0020036f,  0.0026910f, 0.0f, 0.0f) },

    // Fine chop
    { float4(-14.4287370f, 15.1809299f, 20.3437350f, 0.0028648f), float4(-0.0019736f,  0.0020765f, 0.0f, 0.0f) },
    { float4( 9.1501232f, 20.4896679f, 21.9475310f, 0.0028521f), float4( 0.0011630f,  0.0026042f, 0.0f, 0.0f) },
    { float4(-6.9455750f, 24.1539576f, 22.7562643f, 0.0023077f), float4(-0.0006378f,  0.0022179f, 0.0f, 0.0f) },
    { float4(20.3423074f, 18.2338810f, 24.8703927f, 0.0020499f), float4( 0.0015265f,  0.0013682f, 0.0f, 0.0f) },
    { float4(-17.8680458f, 23.9986490f, 25.5140612f, 0.0018048f), float4(-0.0010778f,  0.0014476f, 0.0f, 0.0f) },
    { float4( 3.9588104f, 32.8315823f, 28.0834701f, 0.0015725f), float4( 0.0001882f,  0.0015611f, 0.0f, 0.0f) },
    { float4(-28.9516949f, 22.9746507f, 29.1185632f, 0.0013258f), float4(-0.0010385f,  0.0008241f, 0.0f, 0.0f) },
    { float4(21.1913010f, 33.0613740f, 31.3879508f, 0.0011968f), float4( 0.0006459f,  0.0010076f, 0.0f, 0.0f) },
    { float4(-18.5960548f, 37.5337592f, 31.8095112f, 0.0010743f), float4(-0.0004769f,  0.0009626f, 0.0f, 0.0f) },
    { float4(36.7652316f, 25.7395168f, 34.3940091f, 0.0009581f), float4( 0.0007849f,  0.0005495f, 0.0f, 0.0f) }
};

float3 waves_displace(float2 base_position, float time, uint iterations) {
    float3 displacement = 0.0f;

    for (uint i = 0; i < iterations; ++i) {
        OceanWave wave = sea_waves[i];

        float phase = dot(base_position, wave.phase_data.xy) - wave.phase_data.z * time + wave.displacement_data.z;

        float sin_phase;
        float cos_phase;
        sincos(phase, sin_phase, cos_phase);

        displacement += float3(
            wave.displacement_data.x * cos_phase,
            wave.phase_data.w * sin_phase,
            wave.displacement_data.y * cos_phase
        );
    }

    return displacement;
}

float sinc(float x) {
    return (abs(x) < 0.0001f) ? 1.0f : sin(x) / x;
}

float4 waves_normals(float2 base_position, float time, uint iterations) {
    float3 tangent  = float3(1.0f, 0.0f, 0.0f);
    float3 binormal = float3(0.0f, 0.0f, 1.0f);
    float  height     = 0.0;
    float  max_height = 0.01;

    for (uint i = 0; i < iterations; ++i) {
        OceanWave wave = sea_waves[i];

        float phase = dot(base_position, wave.phase_data.xy) - wave.phase_data.z * time + wave.displacement_data.z;

        float sin_phase;
        float cos_phase;
        sincos(phase, sin_phase, cos_phase);

        float3 phase_derivative = float3(
            -wave.displacement_data.x * sin_phase,
             wave.phase_data.w * cos_phase,
            -wave.displacement_data.y * sin_phase
        );

        height     += wave.phase_data.w * sin_phase + wave.phase_data.w;
        max_height += wave.phase_data.w * 2;

        tangent += phase_derivative * wave.phase_data.x;
        binormal += phase_derivative * wave.phase_data.y;
    }

    return float4(normalize(cross(binormal, tangent)), height / max_height);
}
