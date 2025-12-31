#version 450
#include "tone_mapping.glsl"

layout(set=0,binding=1,std140) uniform Light {
    vec4 LIGHT_POSITION;
	vec4 LIGHT_ENERGY;
};

layout(set=2,binding=0) uniform samplerCube ibl_cubemaps[2];
layout(set=2,binding=1) uniform sampler2D textures[1024];

layout(push_constant) uniform Push {
    uint MATERIAL_INDEX;
} push;

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texCoord;
layout(location=3) in vec3 camera_view;

layout(location=0) out vec4 outColor;

void main() {
	// Sample cubemap for indirect lighting
	vec3 refl = reflect(normalize(camera_view), normalize(normal));
	vec3 hdr = texture(ibl_cubemaps[1], refl).rgb;
	vec3 ldr = aces_approx(hdr);
	outColor = vec4(ldr, 1.0);
	// outColor = vec4(pow(hdr.rgb / (hdr.rgb + vec3(1.0)), vec3(1.0/2.2)), 1.0);
}