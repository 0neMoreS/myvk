#version 450

layout(set = 0, binding = 0) uniform sampler2D aoInput;

layout(set = 1, binding = 0) uniform sampler2D gBufferDepth;
layout(set = 1, binding = 1) uniform sampler2D gBufferNormal;

layout(push_constant) uniform CameraParams {
    float nearPlane;
    float farPlane;
} cam;

layout(location = 0) out vec4 outAO;

const float SIGMA_SPATIAL = 2.0;
const float SIGMA_DEPTH = 0.20;
const float SIGMA_NORMAL = 0.15;

float linearizePerspectiveDepth(float depth01, float nearPlane, float farPlane) {
    float safeNear = max(nearPlane, 1e-5);
    float safeFar = max(farPlane, safeNear + 1e-5);
    return (safeNear * safeFar) / (safeNear + depth01 * (safeFar - safeNear));
}

float gaussian(float x, float sigma) {
    float s = max(sigma, 1e-5);
    return exp(-(x * x) / (2.0 * s * s));
}

void main() {
    vec2 tex_size = vec2(textureSize(aoInput, 0));
    vec2 uv = gl_FragCoord.xy / tex_size;
    vec2 texel_size = 1.0 / tex_size;

    float center_depth01 = texture(gBufferDepth, uv).r;
    float center_depth = linearizePerspectiveDepth(center_depth01, cam.nearPlane, cam.farPlane);
    vec3 center_normal = normalize(texture(gBufferNormal, uv).xyz);

    vec4 result = vec4(0.0);
    float weight_sum = 0.0;
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texel_size;
            vec2 sample_uv = uv + offset;
            if (any(lessThan(sample_uv, vec2(0.0))) || any(greaterThan(sample_uv, vec2(1.0)))) {
                continue;
            }

            float sample_depth01 = texture(gBufferDepth, sample_uv).r;
            float sample_depth = linearizePerspectiveDepth(sample_depth01, cam.nearPlane, cam.farPlane);
            vec3 sample_normal = normalize(texture(gBufferNormal, sample_uv).xyz);

            float w_spatial = gaussian(length(vec2(float(x), float(y))), SIGMA_SPATIAL);
            float w_depth = gaussian(abs(sample_depth - center_depth), SIGMA_DEPTH);
            float n_dot = clamp(dot(center_normal, sample_normal), 0.0, 1.0);
            float w_normal = gaussian(1.0 - n_dot, SIGMA_NORMAL);
            float w = w_spatial * w_depth * w_normal;

            result += texture(aoInput, sample_uv) * w;
            weight_sum += w;
        }
    }

    outAO = result / max(weight_sum, 1e-5);
}

// void main() {
//     vec2 tex_size = vec2(textureSize(aoInput, 0));
//     vec2 uv = gl_FragCoord.xy / tex_size;
//     vec2 texel_size = 1.0 / tex_size;

//     vec4 result = vec4(0.0);
//     for (int x = -2; x < 2; ++x) {
//         for (int y = -2; y < 2; ++y) {
//             vec2 offset = vec2(float(x), float(y)) * texel_size;
//             result += texture(aoInput, uv + offset);
//         }
//     }

//     outAO = result / 16.0;
// }
