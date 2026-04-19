#version 450

layout(set = 0, binding = 0) uniform sampler2D aoInput;

layout(location = 0) out vec4 outAO;

void main() {
    vec2 tex_size = vec2(textureSize(aoInput, 0));
    vec2 uv = gl_FragCoord.xy / tex_size;
    vec2 texel_size = 1.0 / tex_size;

    vec4 result = vec4(0.0);
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texel_size;
            result += texture(aoInput, uv + offset);
        }
    }

    outAO = result / 16.0;
}
