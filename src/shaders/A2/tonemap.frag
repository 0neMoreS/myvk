#version 450
#include "../tone_mapping.glsl"

// HDR texture from the first render pass
layout(set = 0, binding = 0) uniform sampler2D hdrTexture;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main() {
    // Sample HDR color from the offscreen render target
    vec3 hdrColor = texture(hdrTexture, inUV).rgb;

    // Apply ACES tone mapping with exposure = 2.0
    vec3 ldrColor = aces_approx(hdrColor, 2.0);

    // Output tone-mapped color
    outColor = vec4(ldrColor, 1.0);
}
