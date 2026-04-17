#version 450

layout(set = 0, binding = 0) uniform sampler2D gBufferDepth;
layout(set = 0, binding = 1) uniform sampler2D gBufferNormal;
layout(set = 1, binding = 0) uniform sampler2D noiseTexture;

layout(push_constant) uniform Push {
    float RADIUS_PIXELS;
    float DEPTH_BIAS;
    float POWER;
} push;

layout(location = 0) out float outAO;

void main() {
    vec2 tex_size = vec2(textureSize(gBufferDepth, 0));
    vec2 uv = gl_FragCoord.xy / tex_size;

    float center_depth = texture(gBufferDepth, uv).r;
    vec3 normal = normalize(texture(gBufferNormal, uv).xyz);

    if (length(normal) < 1e-4) {
        outAO = 1.0;
        return;
    }

    vec2 texel = 1.0 / tex_size;
    vec2 noise_uv = gl_FragCoord.xy / vec2(textureSize(noiseTexture, 0));
    vec2 noise_xy = normalize(texture(noiseTexture, noise_uv).xy);
    float angle = atan(noise_xy.y, noise_xy.x);
    mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));

    const vec2 kernel[8] = vec2[]( 
        vec2( 1.0,  0.0),
        vec2(-1.0,  0.0),
        vec2( 0.0,  1.0),
        vec2( 0.0, -1.0),
        vec2( 0.7071,  0.7071),
        vec2(-0.7071,  0.7071),
        vec2( 0.7071, -0.7071),
        vec2(-0.7071, -0.7071)
    );

    float occlusion = 0.0;
    for (int i = 0; i < 8; ++i) {
        vec2 offset_dir = rot * kernel[i];
        vec2 sample_uv = clamp(uv + offset_dir * push.RADIUS_PIXELS * texel, vec2(0.0), vec2(1.0));
        float sample_depth = texture(gBufferDepth, sample_uv).r;

        // Fixed Reverse-Z: larger depth is nearer to camera.
        bool blocked = (sample_depth <= center_depth - push.DEPTH_BIAS);

        occlusion += blocked ? 1.0 : 0.0;
    }

    float ao = 1.0 - occlusion / 8.0;
    outAO = clamp(pow(ao, push.POWER), 0.0, 1.0);
}
