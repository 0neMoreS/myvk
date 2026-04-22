#version 450

layout(set = 0, binding = 0) uniform sampler2D aoInput;

layout(location = 0) out float outAO;

const int KERNEL_SIZE = 2;

void main() {
    vec2 tex_size = vec2(textureSize(aoInput, 0));
    vec2 uv = gl_FragCoord.xy / tex_size;
    vec2 texel_size = 1.0 / tex_size;

    float result = 0.0;
    for (int x = -KERNEL_SIZE; x <= KERNEL_SIZE; ++x) {
        for (int y = -KERNEL_SIZE; y <= KERNEL_SIZE; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texel_size;
            result += texture(aoInput, uv + offset).r;
        }
    }

    outAO = result / float((KERNEL_SIZE * 2 + 1) * (KERNEL_SIZE * 2 + 1));
}
