#version 450
#include "../tone_mapping.glsl"

// HDR texture from the first render pass
layout(set = 0, binding = 0) uniform sampler2D hdrTexture;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    float EXPOSURE;
    uint METHOD; // 0: linear, 1: aces
} push;

void main() {
    // Sample HDR color from the offscreen render target
    vec3 hdrColor = texture(hdrTexture, inUV).rgb;

    // Apply tone mapping based on METHOD
    vec3 ldrColor;
    if (push.METHOD == 1) {
        ldrColor = aces_approx(hdrColor, push.EXPOSURE);
    } else {
        ldrColor = linear_tone_map(hdrColor, push.EXPOSURE);
    }

    // Output tone-mapped color
    outColor = vec4(ldrColor, 1.0);
}
