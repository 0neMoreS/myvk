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
	#ifdef USE_TILED_LIGHTING
		uint sphereTileIndex = 0u;
		uint spotTileIndex = 0u;
		uint shadowSphereTileIndex = 0u;
		uint shadowSpotTileIndex = 0u;
		if (sphereTileDataBuf.tiles_x > 0u && sphereTileDataBuf.tiles_y > 0u) {
			uvec2 tile = uvec2(gl_FragCoord.xy) / 16u;
			tile.x = min(tile.x, sphereTileDataBuf.tiles_x - 1u);
			tile.y = min(tile.y, sphereTileDataBuf.tiles_y - 1u);
			sphereTileIndex = tile.y * sphereTileDataBuf.tiles_x + tile.x;
		}
		if (spotTileDataBuf.tiles_x > 0u && spotTileDataBuf.tiles_y > 0u) {
			uvec2 tile = uvec2(gl_FragCoord.xy) / 16u;
			tile.x = min(tile.x, spotTileDataBuf.tiles_x - 1u);
			tile.y = min(tile.y, spotTileDataBuf.tiles_y - 1u);
			spotTileIndex = tile.y * spotTileDataBuf.tiles_x + tile.x;
		}
		if (shadowSphereTileDataBuf.tiles_x > 0u && shadowSphereTileDataBuf.tiles_y > 0u) {
			uvec2 tile = uvec2(gl_FragCoord.xy) / 16u;
			tile.x = min(tile.x, shadowSphereTileDataBuf.tiles_x - 1u);
			tile.y = min(tile.y, shadowSphereTileDataBuf.tiles_y - 1u);
			shadowSphereTileIndex = tile.y * shadowSphereTileDataBuf.tiles_x + tile.x;
		}
		if (shadowSpotTileDataBuf.tiles_x > 0u && shadowSpotTileDataBuf.tiles_y > 0u) {
			uvec2 tile = uvec2(gl_FragCoord.xy) / 16u;
			tile.x = min(tile.x, shadowSpotTileDataBuf.tiles_x - 1u);
			tile.y = min(tile.y, shadowSpotTileDataBuf.tiles_y - 1u);
			shadowSpotTileIndex = tile.y * shadowSpotTileDataBuf.tiles_x + tile.x;
		}
	#endif

		for (uint i = 0u; i < sunLightsBuf.count; ++i) {
			vec3 lightIntensity = sampleSunLightIntensity(sunLightsBuf.lights[i], N);
			Lo += lightIntensity * albedo;
		}

		for (uint i = 0u; i < shadowSunLightsBuf.count; ++i) {
			SunLight light = shadowSunLightsBuf.shadowLights[i];
			vec3 lightIntensity = sampleSunLightIntensity(light, N);
			float shadow = computeSunLightShadow(light, position, viewPosition, sunShadowMap[i]);
			Lo += shadow * lightIntensity * albedo;
		}

	#ifdef USE_TILED_LIGHTING
		TileInfo sphereTileInfo = sphereTileDataBuf.tiles[sphereTileIndex];
		for (uint i = 0u; i < sphereTileInfo.count; ++i) {
			uint lightIndex = sphereLightIdxBuf.indices[sphereTileInfo.offset + i];
			vec3 lightIntensity = sampleSphereLightIntensity(sphereLightsBuf.lights[lightIndex], position, N);
			Lo += lightIntensity * albedo;
		}
	#else
		for (uint i = 0u; i < sphereLightsBuf.count; ++i) {
			vec3 lightIntensity = sampleSphereLightIntensity(sphereLightsBuf.lights[i], position, N);
			Lo += lightIntensity * albedo;
		}
	#endif

	#ifdef USE_TILED_LIGHTING
		TileInfo shadowSphereTileInfo = shadowSphereTileDataBuf.tiles[shadowSphereTileIndex];
		for (uint i = 0u; i < shadowSphereTileInfo.count; ++i) {
			uint lightIndex = shadowSphereLightIdxBuf.indices[shadowSphereTileInfo.offset + i];
			SphereLight light = shadowSphereLightsBuf.shadowLights[lightIndex];
			vec3 lightIntensity = sampleSphereLightIntensity(light, position, N);
			float shadow = computeSphereLightShadow(light, position, sphereShadowMap[lightIndex]);
			Lo += shadow * lightIntensity * albedo;
		}
	#else
		for (uint i = 0u; i < shadowSphereLightsBuf.count; ++i) {
			SphereLight light = shadowSphereLightsBuf.shadowLights[i];
			vec3 lightIntensity = sampleSphereLightIntensity(light, position, N);
			float shadow = computeSphereLightShadow(light, position, sphereShadowMap[i]);
			Lo += shadow * lightIntensity * albedo;
		}
	#endif

	#ifdef USE_TILED_LIGHTING
		TileInfo spotTileInfo = spotTileDataBuf.tiles[spotTileIndex];
		for (uint i = 0u; i < spotTileInfo.count; ++i) {
			uint lightIndex = spotLightIdxBuf.indices[spotTileInfo.offset + i];
			vec3 lightIntensity = sampleSpotLightIntensity(spotLightsBuf.lights[lightIndex], position, N);
			Lo += lightIntensity * albedo;
		}
	#else
		for (uint i = 0u; i < spotLightsBuf.count; ++i) {
			vec3 lightIntensity = sampleSpotLightIntensity(spotLightsBuf.lights[i], position, N);
			Lo += lightIntensity * albedo;
		}
	#endif

	#ifdef USE_TILED_LIGHTING
		TileInfo shadowSpotTileInfo = shadowSpotTileDataBuf.tiles[shadowSpotTileIndex];
		for (uint i = 0u; i < shadowSpotTileInfo.count; ++i) {
			uint lightIndex = shadowSpotLightIdxBuf.indices[shadowSpotTileInfo.offset + i];
			SpotLight light = shadowSpotLightsBuf.shadowLights[lightIndex];
			vec3 lightIntensity = sampleSpotLightIntensity(light, position, N);
			float shadow = computeSpotLightShadow(light, position, spotShadowMap[lightIndex]);
			Lo += shadow * lightIntensity * albedo;
		}
	#else
		for (uint i = 0u; i < shadowSpotLightsBuf.count; ++i) {
			SpotLight light = shadowSpotLightsBuf.shadowLights[i];
			vec3 lightIntensity = sampleSpotLightIntensity(light, position, N);
			float shadow = computeSpotLightShadow(light, position, spotShadowMap[i]);
			Lo += shadow * lightIntensity * albedo;
		}
	#endif
	}

	vec3 color = vec3(0.0);
	{ // indirect lighting
		vec3 irradiance = texture(irradiance_map, N).xyz;
		vec3 diffuse = irradiance * albedo;

		color = Lo / PI + diffuse;
	}
	
	outColor = vec4(color, 1.0);
}