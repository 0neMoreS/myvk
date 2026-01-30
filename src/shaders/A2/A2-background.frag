#version 450
#include "../tone_mapping.glsl"

layout(set=1,binding=0) uniform samplerCube CUBEMAP;

layout(location=0) in vec3 normal;
layout(location=0) out vec4 outColor;

void main() {
    vec3 dir = normalize(-normal);
    dir.x = -dir.x;
    vec3 hdr = texture(CUBEMAP, normalize(normal)).rgb;
    vec3 ldr = aces_approx(hdr);
    outColor = vec4(ldr, 1.0);
}
