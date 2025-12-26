#version 450

layout(set=1,binding=0) uniform samplerCube CUBEMAP;

layout(location=0) in vec3 normal;
layout(location=0) out vec4 outColor;

// ACES tone mapping approximation (Narkowicz 2015), expects linear HDR input
vec3 tonemap_aces(vec3 x) {
    const mat3 ACESInputMat = mat3(
        0.59719, 0.35458, 0.04823,
        0.07600, 0.90834, 0.01566,
        0.02840, 0.13383, 0.83777
    );
    const mat3 ACESOutputMat = mat3(
        1.60475, -0.53108, -0.07367,
       -0.10208,  1.10813, -0.00605,
       -0.00327, -0.07276,  1.07602
    );
    x = ACESInputMat * x;
    x = (x * (x + 0.0245786) - 0.000090537) /
        (x * (0.983729 * x + 0.4329510) + 0.238081);
    x = ACESOutputMat * x;
    return clamp(x, 0.0, 1.0);
}

void main() {
    vec3 dir = normalize(normal);
    vec3 hdr = texture(CUBEMAP, dir).rgb;
    vec3 ldr = tonemap_aces(hdr);
    outColor = vec4(pow(ldr, vec3(1.0/2.2)), 1.0);
}
