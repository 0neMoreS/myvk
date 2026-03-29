#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "SSAO-light-def.glsl"
#include "SSAO-light-intensity.glsl"
#include "SSAO-light-shadow.glsl"

layout(set = 2, binding = 5) uniform sampler2D gBufferPositionDepth; // R32G32B32A32Sfloat: Position + Depth
layout(set = 2, binding = 6) uniform sampler2D gBufferNormal;        // R8G8B8A8Unorm: Normal
layout(set = 2, binding = 7) uniform sampler2D gBufferAlbedo;        // R8G8B8A8Unorm: Albedo
layout(set = 2, binding = 8) uniform sampler2D gBufferPbr;           // R8G8B8A8Unorm: AO + Roughness + Metalness

layout(set=2,binding=0) uniform samplerCube irradiance_map;

layout(location=0) out vec4 outColor;

const float PI = 3.14159265359;

struct GBufferSurface {
	vec3 position;
	float depth;
	vec3 normal;
	vec3 albedo;
	float ao;
};

GBufferSurface sampleGBuffer(vec2 fragCoord) {
	vec2 texSize = vec2(textureSize(gBufferPositionDepth, 0));
	vec2 uv = fragCoord / texSize;

	vec4 posDepth = texture(gBufferPositionDepth, uv);
	vec3 packedNormal = texture(gBufferNormal, uv).xyz;
	vec3 albedo = texture(gBufferAlbedo, uv).xyz;
	vec3 pbr = texture(gBufferPbr, uv).xyz;

	GBufferSurface surface;
	surface.position = posDepth.xyz;
	surface.depth = posDepth.w;
	surface.normal = normalize(packedNormal * 2.0 - 1.0);
	surface.albedo = albedo;
	surface.ao = pbr.x;
	return surface;
}

void main() {
	GBufferSurface g = sampleGBuffer(gl_FragCoord.xy);
	vec3 shadedPosition = g.position;
	vec3 shadedViewPosition = vec3(0.0, 0.0, -g.depth);
	vec3 albedo = g.albedo;
	vec3 N = g.normal;
	float ao = g.ao;

	// reflectance equation
	vec3 Lo = vec3(0.0);

	{ // direct lighting (all lights)
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

	// ============= SUN LIGHTS =============
		for (uint i = 0u; i < sunLightsBuf.count; ++i) {
			vec3 lightIntensity = sampleSunLightIntensity(sunLightsBuf.lights[i]);
			float NoL = sunLightNoLFactor(sunLightsBuf.lights[i], N);
			Lo += lightIntensity * albedo * NoL;
		}

		for (uint i = 0u; i < shadowSunLightsBuf.count; ++i) {
			SunLight light = shadowSunLightsBuf.shadowLights[i];
			vec3 lightIntensity = sampleSunLightIntensity(light);
			float NoL = sunLightNoLFactor(light, N);
			float shadow = computeSunLightShadow(light, shadedPosition, shadedViewPosition, sunShadowMap[i]);
			Lo += shadow * lightIntensity * albedo * NoL;
		}

	// ============= SPHERE LIGHTS =============
		TileInfo sphereTileInfo = sphereTileDataBuf.tiles[sphereTileIndex];
		for (uint i = 0u; i < sphereTileInfo.count; ++i) {
			uint lightIndex = sphereLightIdxBuf.indices[sphereTileInfo.offset + i];
			vec3 lightIntensity = sampleSphereLightIntensity(sphereLightsBuf.lights[lightIndex], shadedPosition, N);
			vec3 toLight = sphereLightsBuf.lights[lightIndex].position - shadedPosition;
			float NoL = areaLightNoLFactor(sphereLightsBuf.lights[lightIndex].radius, toLight, N);
			Lo += lightIntensity * albedo * NoL;
		}

		TileInfo shadowSphereTileInfo = shadowSphereTileDataBuf.tiles[shadowSphereTileIndex];
		for (uint i = 0u; i < shadowSphereTileInfo.count; ++i) {
			uint lightIndex = shadowSphereLightIdxBuf.indices[shadowSphereTileInfo.offset + i];
			SphereLight light = shadowSphereLightsBuf.shadowLights[lightIndex];
			vec3 lightIntensity = sampleSphereLightIntensity(light, shadedPosition, N);
			vec3 toLight = light.position - shadedPosition;
			float NoL = areaLightNoLFactor(light.radius, toLight, N);
			float shadow = computeSphereLightShadow(light, shadedPosition, NoL, sphereShadowMap[lightIndex]);
			Lo += shadow * lightIntensity * albedo * NoL;
		}

	// ============= SPOT LIGHTS =============
		TileInfo spotTileInfo = spotTileDataBuf.tiles[spotTileIndex];
		for (uint i = 0u; i < spotTileInfo.count; ++i) {
			uint lightIndex = spotLightIdxBuf.indices[spotTileInfo.offset + i];
			vec3 lightIntensity = sampleSpotLightIntensity(spotLightsBuf.lights[lightIndex], shadedPosition, N);
			vec3 toLight = spotLightsBuf.lights[lightIndex].position - shadedPosition;
			float NoL = areaLightNoLFactor(spotLightsBuf.lights[lightIndex].radius, toLight, N);
			Lo += lightIntensity * albedo * NoL;
		}

		TileInfo shadowSpotTileInfo = shadowSpotTileDataBuf.tiles[shadowSpotTileIndex];
		for (uint i = 0u; i < shadowSpotTileInfo.count; ++i) {
			uint lightIndex = shadowSpotLightIdxBuf.indices[shadowSpotTileInfo.offset + i];
			SpotLight light = shadowSpotLightsBuf.shadowLights[lightIndex];
			vec3 lightIntensity = sampleSpotLightIntensity(light, shadedPosition, N);
			vec3 toLight = light.position - shadedPosition;
			float NoL = areaLightNoLFactor(light.radius, toLight, N);
			float shadow = computeSpotLightShadow(light, shadedPosition, NoL, spotShadowMap[lightIndex]);
			Lo += shadow * lightIntensity * albedo * NoL;
		}
	}

	vec3 color = vec3(0.0);
	{ // indirect lighting
		vec3 irradiance = texture(irradiance_map, N).xyz;
		vec3 diffuse = irradiance * albedo;

		color = Lo / PI + diffuse * ao;
	}
	
	outColor = vec4(color, 1.0);
}