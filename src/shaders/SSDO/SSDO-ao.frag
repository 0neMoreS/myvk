#version 450

layout(set = 0, binding = 0, std140) uniform PV {
    mat4 PERSPECTIVE;
    mat4 INV_PERSPECTIVE;
    mat4 VIEW;
    vec4 CAMERA_POSITION;
} pv;

layout(set = 1, binding = 0) uniform sampler2D gBufferDepth;
layout(set = 1, binding = 1) uniform sampler2D gBufferNormal;
layout(set = 1, binding = 2) uniform sampler2D gBufferAlbedo;
layout(set = 2, binding = 0) uniform sampler2D noiseTexture;

const float RADIUS_PIXELS = 0.5;
const float DEPTH_BIAS = 0.025;
const float POWER = 1.0;
const float INDIRECT_INTENSITY = 0.5;

layout(location = 0) out vec4 outAO;

const int kernelSize = 64;

layout(set = 0, binding = 1, std140) uniform KernelSamples {
    vec4 samples[kernelSize];
} kernelSamples;

vec3 reconstructViewPosition(vec2 uv, float depth, mat4 invProjection) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view = invProjection * clip;
    return view.xyz / max(view.w, 1e-5);
}

void main() {
    vec2 tex_size = vec2(textureSize(gBufferDepth, 0));
    vec2 uv = gl_FragCoord.xy / tex_size;

    float center_depth = texture(gBufferDepth, uv).r;
    mat4 inv_projection = pv.INV_PERSPECTIVE;
    vec3 frag_pos = reconstructViewPosition(uv, center_depth, inv_projection);
    vec3 normal = normalize(mat3(pv.VIEW) * texture(gBufferNormal, uv).xyz);

    vec2 noise_scale = tex_size / vec2(textureSize(noiseTexture, 0));
    vec3 random_vec = normalize(texture(noiseTexture, uv * noise_scale).xyz);
    vec3 tangent = normalize(random_vec - normal * dot(random_vec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    vec3 indirect = vec3(0.0);
    for (int i = 0; i < kernelSize; ++i) {
        vec3 sample_pos = frag_pos + (tbn * kernelSamples.samples[i].xyz) * RADIUS_PIXELS;

        vec4 offset = pv.PERSPECTIVE * vec4(sample_pos, 1.0);
        offset.xyz /= max(offset.w, 1e-5);
        vec2 sample_uv = offset.xy * 0.5 + 0.5;

        if (any(lessThan(sample_uv, vec2(0.0))) || any(greaterThan(sample_uv, vec2(1.0)))) {
            continue;
        }

        float sample_depth = texture(gBufferDepth, sample_uv).r;
        vec3 sample_view_pos = reconstructViewPosition(sample_uv, sample_depth, inv_projection);

        float range_check = smoothstep(0.0, 1.0, RADIUS_PIXELS / max(abs(frag_pos.z - sample_view_pos.z), 1e-5));
        float occluded = sample_view_pos.z >= sample_pos.z + DEPTH_BIAS ? 1.0 : 0.0;
        float visibility = 1.0 - occluded;
        occlusion += occluded * range_check;

        vec3 sample_albedo = texture(gBufferAlbedo, sample_uv).rgb;
        vec3 sample_vec = sample_view_pos - frag_pos;
        vec3 sample_dir = sample_vec / max(length(sample_vec), 1e-5);
        float n_dot_s = max(dot(normal, sample_dir), 0.0);
        indirect += sample_albedo * n_dot_s * range_check * visibility;
    }

    float ao = 1.0 - occlusion / float(kernelSize);
    vec3 indirect_color = indirect / float(kernelSize);
    float ao_out = clamp(pow(ao, POWER), 0.0, 1.0);
    outAO = vec4(indirect_color * INDIRECT_INTENSITY, ao_out);
}
