#version 450
#include "tone_mapping.glsl"

layout(set=1,binding=0,std140) uniform World {
	vec3 SKY_DIRECTION;
	vec3 SKY_ENERGY; //energy supplied by sky to a surface patch with normal = SKY_DIRECTION
	vec3 SUN_DIRECTION;
	vec3 SUN_ENERGY; //energy supplied by sun to a surface patch with normal = SUN_DIRECTION
};

layout(set=3,binding=0) uniform sampler2D TEXTURE;
layout(set=4,binding=0) uniform samplerCube CUBEMAP;

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texCoord;

layout(location=0) out vec4 outColor;

void main() {
	// Sample cubemap for indirect lighting
	vec3 hdr = texture(CUBEMAP, normalize(normal)).rgb;
	vec3 ldr = aces_approx(hdr);
	outColor = vec4(ldr, 1.0);
	// outColor = vec4(pow(hdr.rgb / (hdr.rgb + vec3(1.0)), vec3(1.0/2.2)), 1.0);
}