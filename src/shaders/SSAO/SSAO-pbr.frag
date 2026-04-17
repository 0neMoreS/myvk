#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "SSAO-light-def.glsl"
#include "SSAO-light-intensity.glsl"
#include "SSAO-light-shadow.glsl"

layout(set = 3, binding = 0) uniform sampler2D gBufferDepth;
layout(set = 3, binding = 1) uniform sampler2D gBufferAlbedo;
layout(set = 3, binding = 2) uniform sampler2D gBufferNormal;
layout(set = 4, binding = 0) uniform sampler2D aoTexture;

layout(set=2,binding=0) uniform samplerCube ibl_cubemaps[2];
layout(set=2,binding=1) uniform sampler2D Textures[];

layout(set=0,binding=0,std140) uniform PV {
	mat4 PERSPECTIVE;
	mat4 VIEW;
	vec4 CAMERA_POSITION;
};

layout(location=0) out vec4 outColor;

const float PI = 3.14159265359;

struct GBufferSurface {
	vec3 position;
	float depth;
	vec3 normal;
	vec3 albedo;
	float ao;
	float roughness;
	float metallic;
};

GBufferSurface sampleGBuffer(vec2 fragCoord) {
	vec2 texSize = vec2(textureSize(gBufferDepth, 0));
	vec2 uv = fragCoord / texSize;

	vec4 albedoMetallic = texture(gBufferAlbedo, uv);
	vec4 normalRoughness = texture(gBufferNormal, uv);
	float depth = texture(gBufferDepth, uv).r;
	mat4 invPV = inverse(PERSPECTIVE * VIEW);
	vec4 world = invPV * vec4(uv * 2.0 - 1.0, depth, 1.0);
	world /= max(world.w, 1e-5);

	GBufferSurface surface;
	surface.position = world.xyz;
	surface.depth = -(VIEW * vec4(surface.position, 1.0)).z;
	surface.normal = normalize(normalRoughness.xyz);
	surface.albedo = albedoMetallic.xyz;
	surface.metallic = albedoMetallic.a;
	surface.ao = texture(aoTexture, uv).r;
	surface.roughness = normalRoughness.a;
	return surface;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 computeSpecularTerm(vec3 N, vec3 V, vec3 L_spec, float NoL_spec, float NdotV, vec3 F0, float roughness, float alpha, float alphaPrime)
{
	vec3 H = normalize(V + L_spec);
	float NDF = DistributionGGX(N, H, roughness);
	float G = GeometrySmith(N, V, L_spec, roughness);
	vec3 F_spec = fresnelSchlick(max(dot(H, V), 0.0), F0);
	vec3 nominator = NDF * G * F_spec;
	float denominator = 4.0 * NdotV * NoL_spec + 0.0001;
	float normalization = (alpha * alpha) / max(alphaPrime * alphaPrime, 1e-5);
	return (nominator / denominator) * normalization;
}

void main() {
	GBufferSurface g = sampleGBuffer(gl_FragCoord.xy);
	vec3 shadedFragPos = g.position;
	// postive view-space depth
	float viewSpaceDepth = g.depth;
	vec3 albedo = g.albedo;
	float roughness = g.roughness;
	float metallic = g.metallic;
	float ao = g.ao;
	vec3 N = g.normal;

	vec3 V = normalize(CAMERA_POSITION.xyz - shadedFragPos);
	vec3 R = reflect(-V, N); 

	// calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
	// of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, albedo, metallic);

	// Pre-compute view-dependent terms shared across all lights
	float NdotV = max(dot(N, V), 0.0);
	vec3 F_diff0 = fresnelSchlick(NdotV, F0);
	vec3 diffuseTerm = (1.0 - metallic) * (vec3(1.0) - F_diff0) * albedo / PI;

	// reflectance equation
	vec3 Lo = vec3(0.0);

	{ // direct lighting 
		float alpha = roughness * roughness;

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

		// --- 1. SUN LIGHTS ---
		for (uint i = 0u; i < sunLightsBuf.count; ++i) {
			SunLight light = sunLightsBuf.lights[i];
			vec3 lightIntensity = sampleSunLightIntensity(light);
			
			// no representative point for directional light
			vec3 L_center = normalize(-light.direction);
			float NoL = sunLightNoLFactor(light, N);
			vec3 specularTerm = computeSpecularTerm(N, V, L_center, NoL, NdotV, F0, roughness, alpha, alpha);

			float shadow = 1.0;
			Lo += shadow * (diffuseTerm + specularTerm) * lightIntensity * NoL;
		}

		// --- 1.1 SHADOW SUN LIGHTS ---
		for (uint i = 0u; i < shadowSunLightsBuf.count; ++i) {
			SunLight light = shadowSunLightsBuf.shadowLights[i];
			vec3 lightIntensity = sampleSunLightIntensity(light);
			
			// no representative point for directional light
			vec3 L_center = normalize(-light.direction);
			float NoL = sunLightNoLFactor(light, N);
			vec3 specularTerm = computeSpecularTerm(N, V, L_center, NoL, NdotV, F0, roughness, alpha, alpha);

			float shadow = computeSunLightShadow(light, shadedFragPos, viewSpaceDepth, sunShadowMap[i]);
			Lo += shadow * (diffuseTerm + specularTerm) * lightIntensity * NoL;
		}

		// --- 2. SPHERE LIGHTS ---
		TileInfo sphereTileInfo = sphereTileDataBuf.tiles[sphereTileIndex];
		for (uint i = 0u; i < sphereTileInfo.count; ++i) {
			uint lightIndex = sphereLightIdxBuf.indices[sphereTileInfo.offset + i];
			SphereLight light = sphereLightsBuf.lights[lightIndex];

			vec3 lightIntensity = sampleSphereLightIntensity(light, shadedFragPos, N);

			vec3 toLight = light.position - shadedFragPos;
			float distToLight = length(toLight);
			vec3 L_center = toLight / max(distToLight, 1e-5);

			// Representative Point for Specular
			vec3 centerToRay = dot(toLight, R) * R - toLight;
			vec3 closestPoint = toLight + centerToRay * clamp(light.radius / max(length(centerToRay), 1e-5), 0.0, 1.0);
			vec3 L_spec = closestPoint / max(length(closestPoint), 1e-5);
			float NoL_spec = areaLightNoLFactor(light.radius, closestPoint, N);
			// float NoL_spec = abs(dot(N, L_spec));
			vec3 specularTerm = computeSpecularTerm(N, V, L_spec, NoL_spec, NdotV, F0, roughness, alpha,
				clamp(alpha + light.radius / (2.0 * distToLight), 0.0, 1.0));

			float NoL_diff = areaLightNoLFactor(light.radius, toLight, N);

			float shadow = 1.0;
			Lo += shadow * (diffuseTerm * NoL_diff + specularTerm * NoL_spec) * lightIntensity;
		}

		// --- 2.1 SHADOW SPHERE LIGHTS ---
		TileInfo shadowSphereTileInfo = shadowSphereTileDataBuf.tiles[shadowSphereTileIndex];
		for (uint i = 0u; i < shadowSphereTileInfo.count; ++i) {
			uint lightIndex = shadowSphereLightIdxBuf.indices[shadowSphereTileInfo.offset + i];
			SphereLight light = shadowSphereLightsBuf.shadowLights[lightIndex];

			vec3 lightIntensity = sampleSphereLightIntensity(light, shadedFragPos, N);

			vec3 toLight = light.position - shadedFragPos;
			float distToLight = length(toLight);
			vec3 L_center = toLight / max(distToLight, 1e-5);

			vec3 centerToRay = dot(toLight, R) * R - toLight;
			vec3 closestPoint = toLight + centerToRay * clamp(light.radius / max(length(centerToRay), 1e-5), 0.0, 1.0);
			vec3 L_spec = closestPoint / max(length(closestPoint), 1e-5);
			float NoL_spec = areaLightNoLFactor(light.radius, closestPoint, N);
			// float NoL_spec = abs(dot(N, L_spec), 0.0);
			vec3 specularTerm = computeSpecularTerm(N, V, L_spec, NoL_spec, NdotV, F0, roughness, alpha,
				clamp(alpha + light.radius / (2.0 * distToLight), 0.0, 1.0));

			float NoL_diff = areaLightNoLFactor(light.radius, toLight, N);

			float shadow = computeSphereLightShadow(light, shadedFragPos, NoL_diff, sphereShadowMap[lightIndex]);
			Lo += shadow * (diffuseTerm * NoL_diff + specularTerm * NoL_spec) * lightIntensity;
		}

		// --- 3. SPOT LIGHTS ---
		TileInfo spotTileInfo = spotTileDataBuf.tiles[spotTileIndex];
		for (uint i = 0u; i < spotTileInfo.count; ++i) {
			uint lightIndex = spotLightIdxBuf.indices[spotTileInfo.offset + i];
			SpotLight light = spotLightsBuf.lights[lightIndex];

			vec3 lightIntensity = sampleSpotLightIntensity(light, shadedFragPos, N);

			vec3 toLight = light.position - shadedFragPos;
			float distToLight = length(toLight);
			vec3 L_center = toLight / max(distToLight, 1e-5);

			// Representative Point for Specular
			vec3 centerToRay = dot(toLight, R) * R - toLight;
			vec3 closestPoint = toLight + centerToRay * clamp(light.radius / max(length(centerToRay), 1e-5), 0.0, 1.0);
			vec3 L_spec = closestPoint / max(length(closestPoint), 1e-5);
			float NoL_spec = areaLightNoLFactor(light.radius, closestPoint, N);
			vec3 specularTerm = computeSpecularTerm(N, V, L_spec, NoL_spec, NdotV, F0, roughness, alpha,
				clamp(alpha + light.radius / (2.0 * distToLight), 0.0, 1.0));

			float NoL_diff = areaLightNoLFactor(light.radius, toLight, N);

			float shadow = 1.0;
			Lo += shadow * (diffuseTerm * NoL_diff + specularTerm * NoL_spec) * lightIntensity;
		}

		// --- 3.1. SHADOW SPOT LIGHTS ---
		TileInfo shadowSpotTileInfo = shadowSpotTileDataBuf.tiles[shadowSpotTileIndex];
		for (uint i = 0u; i < shadowSpotTileInfo.count; ++i) {
			uint lightIndex = shadowSpotLightIdxBuf.indices[shadowSpotTileInfo.offset + i];
			SpotLight light = shadowSpotLightsBuf.shadowLights[lightIndex];

			vec3 lightIntensity = sampleSpotLightIntensity(light, shadedFragPos, N);

			vec3 toLight = light.position - shadedFragPos;
			float distToLight = length(toLight);
			vec3 L_center = toLight / max(distToLight, 1e-5);

			vec3 centerToRay = dot(toLight, R) * R - toLight;
			vec3 closestPoint = toLight + centerToRay * clamp(light.radius / max(length(centerToRay), 1e-5), 0.0, 1.0);
			vec3 L_spec = closestPoint / max(length(closestPoint), 1e-5);
			float NoL_spec = areaLightNoLFactor(light.radius, closestPoint, N);

			vec3 specularTerm = computeSpecularTerm(N, V, L_spec, NoL_spec, NdotV, F0, roughness, alpha,
				clamp(alpha + light.radius / (2.0 * distToLight), 0.0, 1.0));

			float NoL_diff = areaLightNoLFactor(light.radius, toLight, N);

			float shadow = computeSpotLightShadow(light, shadedFragPos, NoL_diff, spotShadowMap[lightIndex]);
			Lo += shadow * (diffuseTerm * NoL_diff + specularTerm * NoL_spec) * lightIntensity;
		}
	}

	vec3 color = vec3(0.0);
	{ // indirect lighting

		// ambient lighting (we now use IBL as the ambient term)
		vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);

		vec3 kS = F;
		vec3 kD = 1.0 - kS;
		kD *= (1.0 - metallic);	  

		vec3 irradiance = texture(ibl_cubemaps[0], N).xyz;
		vec3 diffuse = irradiance * albedo;

		// sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
		const float MAX_REFLECTION_LOD = 4.0;
		vec3 prefilteredColor = textureLod(ibl_cubemaps[1], R,  roughness * MAX_REFLECTION_LOD).xyz;    
		vec2 brdf = texture(Textures[nonuniformEXT(0)], vec2(NdotV, roughness)).xy;
		vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

		vec3 fakeAmbient = vec3(0.02, 0.02, 0.02);

		vec3 ambient = (kD * diffuse + specular ) * ao;

		color = ambient + Lo;
	}

	outColor = vec4(color, 1.0);
}