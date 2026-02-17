#version 450
#include "../tone_mapping.glsl"

layout(set=0,binding=1,std140) uniform Light {
    vec4 LIGHT_POSITION;
	vec4 LIGHT_ENERGY;
	vec4 CAMERA_POSITION;
};

layout(set=2,binding=0) uniform samplerCube CUBEMAP;

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texCoord;
layout(location=3) flat in float reflective;

layout(location=0) out vec4 outColor;

void main() {
	vec3 refl = vec3(0.0);
	if(reflective == 1.0){
		refl = reflect(normalize(position - CAMERA_POSITION.xyz), normalize(normal));
	} else {
		refl = normalize(normal);
	}
	vec3 hdr = texture(CUBEMAP, refl).rgb;
	outColor = vec4(hdr, 1.0);
}