#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "A3-light-def.glsl"
#include "A3-light-intensity.glsl"

layout(set=2,binding=0) uniform samplerCube ibl_cubemaps[2];
layout(set=2,binding=1) uniform sampler2D Textures[];

layout(push_constant) uniform Push {
    uint MATERIAL_INDEX;
} push;

layout(location=0) in vec3 fragPos;
layout(location=1) in vec2 texCoord;
layout(location=2) flat in vec3 cameraPos;
layout(location=3) in mat3 TBN;

layout(location=0) out vec4 outColor;

const float PI = 3.14159265359;

vec2 ParallaxMapping(vec3 viewDir)
{ 
    // number of depth layers
    const float minLayers = 8;
    const float maxLayers = 16;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir))); 
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy / viewDir.z * 0.001; // scale height
    vec2 deltaTexCoords = P / numLayers;
	float scale = 1.0; // scale height
  
    // get initial values
    vec2  currentTexCoords = texCoord;
    float currentDepthMapValue = scale * texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 1)], currentTexCoords).r;
      
    while(currentLayerDepth < currentDepthMapValue)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = scale * texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 1)], currentTexCoords).r;  
        // get depth of next layer
        currentLayerDepth += layerDepth;  
    }
    
    // get texture coordinates before collision (reverse operations)
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = scale * texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 1)], prevTexCoords).r - currentLayerDepth + layerDepth;
 
    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

vec3 getNormalFromMap(vec2 mappedTexCoord)
{
    vec3 tangentNormal = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX)], mappedTexCoord).xyz * 2.0 - 1.0;
    return normalize(TBN * tangentNormal);
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

void main() {
	// offset texture coordinates with Parallax Mapping
	vec3 viewDir = normalize(transpose(TBN) * (cameraPos -  fragPos));
	vec2 mappedTexCoord = ParallaxMapping(viewDir);       
	// if(mappedTexCoord.x > 1.0 || mappedTexCoord.y > 1.0 || mappedTexCoord.x < 0.0 || mappedTexCoord.y < 0.0){
	// 	discard; 
	// }

	// material properties
	vec3 albedo = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 2)], mappedTexCoord).xyz;
	float roughness = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 3)], mappedTexCoord).x;
	float metallic = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 4)], mappedTexCoord).x;

	// input lighting data
	vec3 N = getNormalFromMap(mappedTexCoord);
	vec3 V = normalize(cameraPos - fragPos);
	vec3 R = reflect(-V, N); 

	// calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
	// of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, albedo, metallic);

	// reflectance equation
	vec3 Lo = vec3(0.0);

	// { // direct lighting (all lights)
	// 	for (uint i = 0u; i < sunLightsBuf.count; ++i) {
	// 		LightSample ls = sampleSunLightIntensity(sunLightsBuf.lights[i], N);
	// 		if (ls.NoL <= 0.0) continue;

	// 		vec3 H = normalize(V + normalize(sunLightsBuf.lights[i].direction));
	// 		float NDF = DistributionGGX(N, H, roughness);
	// 		float G = GeometrySmith(N, V, normalize(sunLightsBuf.lights[i].direction), roughness);
	// 		vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

	// 		vec3 numerator = NDF * G * F;
	// 		float denominator = 4.0 * max(dot(N, V), 0.0) * ls.NoL + 0.0001;
	// 		vec3 specular = numerator / denominator;

	// 		vec3 kS = F;
	// 		vec3 kD = vec3(1.0) - kS;
	// 		kD *= (1.0 - metallic);

	// 		Lo += (kD * albedo / PI + specular) * ls.intensity * ls.NoL;
	// 	}

	// 	for (uint i = 0u; i < sphereLightsBuf.count; ++i) {
	// 		LightSample ls = sampleSphereLightIntensity(sphereLightsBuf.lights[i], fragPos, N);
	// 		if (ls.NoL <= 0.0) continue;

	// 		vec3 L = normalize(sphereLightsBuf.lights[i].position - fragPos);
	// 		vec3 H = normalize(V + L);
	// 		float NDF = DistributionGGX(N, H, roughness);
	// 		float G = GeometrySmith(N, V, L, roughness);
	// 		vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

	// 		vec3 numerator = NDF * G * F;
	// 		float denominator = 4.0 * max(dot(N, V), 0.0) * ls.NoL + 0.0001;
	// 		vec3 specular = numerator / denominator;

	// 		vec3 kS = F;
	// 		vec3 kD = vec3(1.0) - kS;
	// 		kD *= (1.0 - metallic);

	// 		Lo += (kD * albedo / PI + specular) * ls.intensity * ls.NoL;
	// 	}

	// 	for (uint i = 0u; i < spotLightsBuf.count; ++i) {
	// 		LightSample ls = sampleSpotLightIntensity(spotLightsBuf.lights[i], fragPos, N);
	// 		if (ls.NoL <= 0.0) continue;

	// 		vec3 L = normalize(spotLightsBuf.lights[i].position - fragPos);
	// 		vec3 H = normalize(V + L);
	// 		float NDF = DistributionGGX(N, H, roughness);
	// 		float G = GeometrySmith(N, V, L, roughness);
	// 		vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

	// 		vec3 numerator = NDF * G * F;
	// 		float denominator = 4.0 * max(dot(N, V), 0.0) * ls.NoL + 0.0001;
	// 		vec3 specular = numerator / denominator;

	// 		vec3 kS = F;
	// 		vec3 kD = vec3(1.0) - kS;
	// 		kD *= (1.0 - metallic);

	// 		Lo += (kD * albedo / PI + specular) * ls.intensity * ls.NoL;
	// 	}
	// }

	{ // direct lighting 
		float alpha = roughness * roughness;

		// --- 1. SUN LIGHTS ---
		for (uint i = 0u; i < sunLightsBuf.count; ++i) {
			SunLight light = sunLightsBuf.lights[i];
			LightSample ls = sampleSunLightIntensity(light, N);
			
			vec3 L_center = normalize(light.direction);
			
			// Representative Point for Specular
			vec3 centerToRay = dot(L_center, R) * R - L_center;
			float sunRadius = sin(light.angle * 0.5); 
			vec3 closestPoint = L_center + centerToRay * clamp(sunRadius / max(length(centerToRay), 1e-5), 0.0, 1.0);
			vec3 L_spec = normalize(closestPoint);
			
			float NoL_spec = max(dot(N, L_spec), 0.0);

			if (ls.NoL > 0.0 || NoL_spec > 0.0) {
				// Diffuse (Using Light Center)
				vec3 F_diff = fresnelSchlick(max(dot(N, V), 0.0), F0);
				vec3 kD = (1.0 - metallic) * (vec3(1.0) - F_diff);
				vec3 diffuseTerm = (kD * albedo / PI) * ls.NoL;

				// Specular (Using Representative Point)
				vec3 specularTerm = vec3(0.0);
				if (NoL_spec > 0.0) {
					vec3 H = normalize(V + L_spec);
					float NDF = DistributionGGX(N, H, roughness);
					float G = GeometrySmith(N, V, L_spec, roughness);
					vec3 F_spec = fresnelSchlick(max(dot(H, V), 0.0), F0);

					vec3 nominator = NDF * G * F_spec;
					float denominator = 4.0 * max(dot(N, V), 0.0) * NoL_spec + 0.0001;
					vec3 specular = nominator / denominator;

					// Epic's Energy Normalization Factor
					float alphaPrime = clamp(alpha + light.angle * 0.25, 0.0, 1.0);
					float normalization = (alpha * alpha) / max(alphaPrime * alphaPrime, 1e-5);
					
					specularTerm = specular * normalization * NoL_spec;
				}

				Lo += (diffuseTerm + specularTerm) * ls.intensity;
			}
		}

		// --- 2. SPHERE LIGHTS ---
		for (uint i = 0u; i < sphereLightsBuf.count; ++i) {
			SphereLight light = sphereLightsBuf.lights[i];
			LightSample ls = sampleSphereLightIntensity(light, fragPos, N);

			vec3 toLight = light.position - fragPos;
			float distToLight = length(toLight);
			vec3 L_center = toLight / max(distToLight, 1e-5);

			// Representative Point for Specular
			vec3 centerToRay = dot(toLight, R) * R - toLight;
			vec3 closestPoint = toLight + centerToRay * clamp(light.radius / max(length(centerToRay), 1e-5), 0.0, 1.0);
			vec3 L_spec = normalize(closestPoint);

			float NoL_spec = max(dot(N, L_spec), 0.0);

			if (ls.NoL > 0.0 || NoL_spec > 0.0) {
				// Diffuse
				vec3 F_diff = fresnelSchlick(max(dot(N, V), 0.0), F0);
				vec3 kD = (1.0 - metallic) * (vec3(1.0) - F_diff);
				vec3 diffuseTerm = (kD * albedo / PI) * ls.NoL;

				// Specular
				vec3 specularTerm = vec3(0.0);
				if (NoL_spec > 0.0) {
					vec3 H = normalize(V + L_spec);
					float NDF = DistributionGGX(N, H, roughness);
					float G = GeometrySmith(N, V, L_spec, roughness);
					vec3 F_spec = fresnelSchlick(max(dot(H, V), 0.0), F0);

					vec3 nominator = NDF * G * F_spec;
					float denominator = 4.0 * max(dot(N, V), 0.0) * NoL_spec + 0.0001;
					vec3 specular = nominator / denominator;

					// Epic's Energy Normalization Factor
					float alphaPrime = clamp(alpha + light.radius / (2.0 * distToLight), 0.0, 1.0);
					float normalization = (alpha * alpha) / max(alphaPrime * alphaPrime, 1e-5);
					
					specularTerm = specular * normalization * NoL_spec;
				}

				Lo += (diffuseTerm + specularTerm) * ls.intensity;
			}
		}

		// --- 3. SPOT LIGHTS ---
		for (uint i = 0u; i < spotLightsBuf.count; ++i) {
			SpotLight light = spotLightsBuf.lights[i];
			LightSample ls = sampleSpotLightIntensity(light, fragPos, N);

			vec3 toLight = light.position - fragPos;
			float distToLight = length(toLight);
			vec3 L_center = toLight / max(distToLight, 1e-5);

			// Representative Point for Specular
			vec3 centerToRay = dot(toLight, R) * R - toLight;
			vec3 closestPoint = toLight + centerToRay * clamp(light.radius / max(length(centerToRay), 1e-5), 0.0, 1.0);
			vec3 L_spec = normalize(closestPoint);

			float NoL_spec = max(dot(N, L_spec), 0.0);

			if (ls.NoL > 0.0 || NoL_spec > 0.0) {
				// Diffuse
				vec3 F_diff = fresnelSchlick(max(dot(N, V), 0.0), F0);
				vec3 kD = (1.0 - metallic) * (vec3(1.0) - F_diff);
				vec3 diffuseTerm = (kD * albedo / PI) * ls.NoL;

				// Specular
				vec3 specularTerm = vec3(0.0);
				if (NoL_spec > 0.0) {
					vec3 H = normalize(V + L_spec);
					float NDF = DistributionGGX(N, H, roughness);
					float G = GeometrySmith(N, V, L_spec, roughness);
					vec3 F_spec = fresnelSchlick(max(dot(H, V), 0.0), F0);

					vec3 nominator = NDF * G * F_spec;
					float denominator = 4.0 * max(dot(N, V), 0.0) * NoL_spec + 0.0001;
					vec3 specular = nominator / denominator;

					// Epic's Energy Normalization Factor
					float alphaPrime = clamp(alpha + light.radius / (2.0 * distToLight), 0.0, 1.0);
					float normalization = (alpha * alpha) / max(alphaPrime * alphaPrime, 1e-5);
					
					specularTerm = specular * normalization * NoL_spec;
				}

				Lo += (diffuseTerm + specularTerm) * ls.intensity;
			}
		}

		// --- 4. SHADOW SPOT LIGHTS ---
		for (uint i = 0u; i < shadowSpotLightsBuf.count; ++i) {
			SpotLight light = shadowSpotLightsBuf.shadowLights[i];
			LightSample ls = sampleSpotLightIntensity(light, fragPos, N);

			vec3 toLight = light.position - fragPos;
			float distToLight = length(toLight);
			vec3 L_center = toLight / max(distToLight, 1e-5);

			vec3 centerToRay = dot(toLight, R) * R - toLight;
			vec3 closestPoint = toLight + centerToRay * clamp(light.radius / max(length(centerToRay), 1e-5), 0.0, 1.0);
			vec3 L_spec = normalize(closestPoint);

			float NoL_spec = max(dot(N, L_spec), 0.0);

			if (ls.NoL > 0.0 || NoL_spec > 0.0) {
				vec3 F_diff = fresnelSchlick(max(dot(N, V), 0.0), F0);
				vec3 kD = (1.0 - metallic) * (vec3(1.0) - F_diff);
				vec3 diffuseTerm = (kD * albedo / PI) * ls.NoL;

				vec3 specularTerm = vec3(0.0);
				if (NoL_spec > 0.0) {
					vec3 H = normalize(V + L_spec);
					float NDF = DistributionGGX(N, H, roughness);
					float G = GeometrySmith(N, V, L_spec, roughness);
					vec3 F_spec = fresnelSchlick(max(dot(H, V), 0.0), F0);

					vec3 nominator = NDF * G * F_spec;
					float denominator = 4.0 * max(dot(N, V), 0.0) * NoL_spec + 0.0001;
					vec3 specular = nominator / denominator;

					float alphaPrime = clamp(alpha + light.radius / (2.0 * distToLight), 0.0, 1.0);
					float normalization = (alpha * alpha) / max(alphaPrime * alphaPrime, 1e-5);

					specularTerm = specular * normalization * NoL_spec;
				}

				float shadow = computeSpotLightShadow(light, fragPos, spotShadowMap[i]);
				Lo += shadow * (diffuseTerm + specularTerm) * ls.intensity;
			}
		}
	}

	vec3 color = vec3(0.0);
	{ // indirect lighting

		// ambient lighting (we now use IBL as the ambient term)
		vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

		vec3 kS = F;
		vec3 kD = 1.0 - kS;
		kD *= (1.0 - metallic);	  

		vec3 irradiance = texture(ibl_cubemaps[0], N).xyz;
		vec3 diffuse = irradiance * albedo;

		// sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
		const float MAX_REFLECTION_LOD = 4.0;
		vec3 prefilteredColor = textureLod(ibl_cubemaps[1], R,  roughness * MAX_REFLECTION_LOD).xyz;    
		vec2 brdf = texture(Textures[nonuniformEXT(0)], vec2(max(dot(N, V), 0.0), roughness)).xy;
		vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

		vec3 ambient = kD * diffuse + specular;

		color = ambient + Lo;
	}

	outColor = vec4(color, 1.0);
}