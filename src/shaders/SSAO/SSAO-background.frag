#version 450

layout(set=1,binding=0) uniform samplerCube CUBEMAP;

layout(location=0) in vec3 normal;
layout(location=0) out vec4 outColor;

void main() {
    vec3 hdr = texture(CUBEMAP, normalize(normal)).rgb;
    outColor = vec4(hdr, 1.0);
}
