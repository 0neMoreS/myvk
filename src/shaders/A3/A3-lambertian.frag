#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "A3-lights-def.glsl"

layout(set=2,binding=0) uniform samplerCube irradiance_map;
layout(set=2,binding=1) uniform sampler2D Textures[];

layout(push_constant) uniform Push {
    uint MATERIAL_INDEX;
} push;

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texCoord;

layout(location=0) out vec4 outColor;

const float PI = 3.14159265359;

void main() {
	// material properties
	vec3 albedo = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 2)], texCoord).xyz;

	// input lighting data
	vec3 N = normalize(normal);

	// reflectance equation
	vec3 Lo = vec3(0.0);

	// { // direct lighting
	// 	// calculate per-light radiance
	// 	vec3 L = normalize(LIGHT_POSITION.xyz - position);
	// 	float NoL = max(dot(N, L), 0.0);
	// 	float distance = length(LIGHT_POSITION.xyz - position);
	// 	float attenuation = 1.0 / (distance * distance);
	// 	vec3 radiance = LIGHT_ENERGY.xyz * attenuation;
	// 	Lo += radiance * albedo * NoL / PI;
	// }

	vec3 color = vec3(0.0);
	{ // indirect lighting
		vec3 irradiance = texture(irradiance_map, N).xyz;
		vec3 diffuse = irradiance * albedo;

		color = Lo + diffuse;
	}
	
	outColor = vec4(color, 1.0);
}