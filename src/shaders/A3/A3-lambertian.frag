#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "A3-light-def.glsl"
#include "A3-light-intensity.glsl"
#include "A3-light-shadow.glsl"

layout(set=2,binding=0) uniform samplerCube irradiance_map;
layout(set=2,binding=1) uniform sampler2D Textures[];

layout(push_constant) uniform Push {
    uint MATERIAL_INDEX;
} push;

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texCoord;
layout(location=3) in vec3 viewPosition;

layout(location=0) out vec4 outColor;

const float PI = 3.14159265359;

void main() {
	// material properties
	vec3 albedo = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 2)], texCoord).xyz;

	// input lighting data
	vec3 N = normalize(normal);

	// reflectance equation
	vec3 Lo = vec3(0.0);

	{ // direct lighting (all lights)
		for (uint i = 0u; i < sunLightsBuf.count; ++i) {
			LightSample ls = sampleSunLightIntensity(sunLightsBuf.lights[i], N);
			if (ls.NoL > 0.0) {
				Lo += ls.intensity * albedo * ls.NoL;
			}
		}

		for (uint i = 0u; i < shadowSunLightsBuf.count; ++i) {
			SunLight light = shadowSunLightsBuf.shadowLights[i];
			LightSample ls = sampleSunLightIntensity(light, N);
			if (ls.NoL > 0.0) {
				float shadow = computeSunLightShadow(light, position, viewPosition, sunShadowMap[i]);
				Lo += shadow * ls.intensity * albedo * ls.NoL;
			}
		}

		for (uint i = 0u; i < sphereLightsBuf.count; ++i) {
			LightSample ls = sampleSphereLightIntensity(sphereLightsBuf.lights[i], position, N);
			if (ls.NoL > 0.0) {
				Lo += ls.intensity * albedo * ls.NoL;
			}
		}

		for (uint i = 0u; i < shadowSphereLightsBuf.count; ++i) {
			SphereLight light = shadowSphereLightsBuf.shadowLights[i];
			LightSample ls = sampleSphereLightIntensity(light, position, N);
			if (ls.NoL > 0.0) {
				float shadow = computeSphereLightShadow(light, position, sphereShadowMap[i]);
				Lo += shadow * ls.intensity * albedo * ls.NoL;
			}
		}

		for (uint i = 0u; i < spotLightsBuf.count; ++i) {
			LightSample ls = sampleSpotLightIntensity(spotLightsBuf.lights[i], position, N);
			if (ls.NoL > 0.0) {
				Lo += ls.intensity * albedo * ls.NoL;
			}
		}

		for (uint i = 0u; i < shadowSpotLightsBuf.count; ++i) {
			SpotLight light = shadowSpotLightsBuf.shadowLights[i];
			LightSample ls = sampleSpotLightIntensity(light, position, N);
			if (ls.NoL > 0.0) {
				float shadow = computeSpotLightShadow(light, position, spotShadowMap[i]);
				Lo += shadow * ls.intensity * albedo * ls.NoL;
			}
		}
	}

	vec3 color = vec3(0.0);
	{ // indirect lighting
		vec3 irradiance = texture(irradiance_map, N).xyz;
		vec3 diffuse = irradiance * albedo;

		color = Lo / PI + diffuse;
	}
	
	outColor = vec4(color, 1.0);
}