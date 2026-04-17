#version 450

layout(set = 0, binding = 0, std140) uniform PV {
    mat4 PERSPECTIVE;
    mat4 VIEW;
    vec4 CAMERA_POSITION;
} pv;

layout(set = 1, binding = 0) uniform sampler2D gBufferDepth;
layout(set = 1, binding = 1) uniform sampler2D gBufferNormal;
layout(set = 2, binding = 0) uniform sampler2D noiseTexture;

layout(push_constant) uniform Push {
    float RADIUS_PIXELS;
    float DEPTH_BIAS;
    float POWER;
} push;

layout(location = 0) out float outAO;

const int kernelSize = 64;

vec3 sampleKernel(int index) {
    float fi = float(index);
    float scale = fi / float(kernelSize);
    scale = mix(0.1, 1.0, scale * scale);

    float r1 = fract(sin(fi * 12.9898 + 78.233) * 43758.5453);
    float r2 = fract(sin(fi * 39.3467 + 11.135) * 24634.6345);
    float phi = r1 * 6.28318530718;
    float z = r2;
    float xy = sqrt(max(0.0, 1.0 - z * z));

    return vec3(cos(phi) * xy, sin(phi) * xy, z) * scale;
}

vec3 reconstructViewPosition(vec2 uv, float depth, mat4 invProjection) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view = invProjection * clip;
    return view.xyz / max(view.w, 1e-5);
}

void main() {
    vec2 tex_size = vec2(textureSize(gBufferDepth, 0));
    vec2 uv = gl_FragCoord.xy / tex_size;

    float center_depth = texture(gBufferDepth, uv).r;
    mat4 inv_projection = inverse(pv.PERSPECTIVE);
    vec3 frag_pos = reconstructViewPosition(uv, center_depth, inv_projection);
    vec3 normal = normalize(mat3(pv.VIEW) * texture(gBufferNormal, uv).xyz);

    if (length(normal) < 1e-4) {
        outAO = 1.0;
        return;
    }

    vec2 noise_scale = tex_size / vec2(textureSize(noiseTexture, 0));
    vec3 random_vec = normalize(texture(noiseTexture, uv * noise_scale).xyz);
    vec3 tangent = normalize(random_vec - normal * dot(random_vec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < kernelSize; ++i) {
        vec3 sample_pos = frag_pos + (tbn * sampleKernel(i)) * push.RADIUS_PIXELS;

        vec4 offset = pv.PERSPECTIVE * vec4(sample_pos, 1.0);
        offset.xyz /= max(offset.w, 1e-5);
        vec2 sample_uv = offset.xy * 0.5 + 0.5;

        if (any(lessThan(sample_uv, vec2(0.0))) || any(greaterThan(sample_uv, vec2(1.0)))) {
            continue;
        }

        float sample_depth = texture(gBufferDepth, sample_uv).r;
        vec3 sample_view_pos = reconstructViewPosition(sample_uv, sample_depth, inv_projection);

        float range_check = smoothstep(0.0, 1.0, push.RADIUS_PIXELS / max(abs(frag_pos.z - sample_view_pos.z), 1e-5));
        occlusion += (sample_view_pos.z >= sample_pos.z + push.DEPTH_BIAS ? 1.0 : 0.0) * range_check;
    }

    float ao = 1.0 - occlusion / float(kernelSize);
    outAO = clamp(pow(ao, push.POWER), 0.0, 1.0);
}
