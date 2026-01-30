#version 450
#extension GL_EXT_nonuniform_qualifier : require
#include "../tone_mapping.glsl"

layout(set=0,binding=1,std140) uniform Light {
    vec4 LIGHT_POSITION;
	vec4 LIGHT_ENERGY;
	vec4 CAMERA_POSITION;
};

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

vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX)], texCoord).xyz * 2.0 - 1.0; // sample nomal map

    vec3 Q1  = dFdx(position);
    vec3 Q2  = dFdy(position);
    vec2 st1 = dFdx(texCoord);
    vec2 st2 = dFdy(texCoord);

    vec3 N   = normalize(normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main() {
	// material properties
	vec3 albedo = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 2)], texCoord).xyz;

	// input lighting data
	vec3 N = normalize(normal);

	// reflectance equation
	vec3 Lo = vec3(0.0);

	{ // direct lighting
		// calculate per-light radiance
		vec3 L = normalize(LIGHT_POSITION.xyz - position);
		float NoL = max(dot(N, L), 0.0);
		float distance = length(LIGHT_POSITION.xyz - position);
		float attenuation = 1.0 / (distance * distance);
		vec3 radiance = LIGHT_ENERGY.xyz * attenuation;
		Lo += radiance * albedo * NoL;
	}

	vec3 color = vec3(0.0);
	{ // indirect lighting
		vec3 irradiance = texture(irradiance_map, N).xyz;
		vec3 diffuse = irradiance * albedo;

		color = diffuse + Lo;
	}

	// HDR tonemapping
	vec3 ldr = aces_approx(color);
	// outColor = vec4(pow(ldr, vec3(1.0/2.2)), 1.0);
	outColor = vec4(ldr, 1.0);
}