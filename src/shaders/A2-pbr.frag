#version 450
#extension GL_EXT_nonuniform_qualifier : require
#include "tone_mapping.glsl"

layout(set=0,binding=1,std140) uniform Light {
    vec4 LIGHT_POSITION;
	vec4 LIGHT_ENERGY;
};

layout(set=2,binding=0) uniform samplerCube ibl_cubemaps[2];
layout(set=2,binding=1) uniform sampler2D Textures[];

layout(push_constant) uniform Push {
    uint MATERIAL_INDEX;
} push;

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texCoord;
layout(location=3) in vec3 camera_view;

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
	// material properties
	vec3 albedo = pow(texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 2)], texCoord).xyz, vec3(2.2));
	float roughness = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 3)], texCoord).x;
	float metallic = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 4)], texCoord).x;

	// input lighting data
	vec3 N = getNormalFromMap();
	vec3 V = normalize(camera_view);
	vec3 R = reflect(-V, N); 

	// calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
	// of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, albedo, metallic);

	// reflectance equation
	vec3 Lo = vec3(0.0);

	{ // direct lighting
		// calculate per-light radiance
		vec3 L = normalize(LIGHT_POSITION.xyz - position);
		vec3 H = normalize(V + L);
		float distance = length(LIGHT_POSITION.xyz - position);
		float attenuation = 1.0 / (distance * distance);
		vec3 radiance = LIGHT_ENERGY.xyz * attenuation;
		// Cook-Torrance BRDF
		float NDF = DistributionGGX(N, H, roughness);   
		float G = GeometrySmith(N, V, L, roughness);    
		vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);        
		
		vec3 numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
		vec3 specular = numerator / denominator;
		
		// kS is equal to Fresnel
		vec3 kS = F;
		// for energy conservation, the diffuse and specular light can't
		// be above 1.0 (unless the surface emits light); to preserve this
		// relationship the diffuse component (kD) should equal 1.0 - kS.
		vec3 kD = vec3(1.0) - kS;
		// multiply kD by the inverse metalness such that only non-metals 
		// have diffuse lighting, or a linear blend if partly metal (pure metals
		// have no diffuse light).
		kD *= (1.0 - metallic);	                
			
		// scale light by NdotL
		float NdotL = max(dot(N, L), 0.0);        

		// add to outgoing radiance Lo
		Lo += (kD * albedo / PI + specular) * radiance * NdotL; // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
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
		const float MAX_REFLECTION_LOD = 5.0;
		vec3 prefilteredColor = textureLod(ibl_cubemaps[1], R,  roughness * MAX_REFLECTION_LOD).xyz;    
		vec2 brdf  = texture(Textures[nonuniformEXT(0)], vec2(max(dot(N, V), 0.0), roughness)).rg;
		vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

		vec3 ambient = kD * diffuse + specular;

		color = ambient + Lo;
	}

	// HDR tonemapping
	vec3 ldr = aces_approx(color);
	outColor = vec4(pow(ldr, vec3(1.0/2.2)), 1.0);
	// outColor = vec4(texture(Textures[nonuniformEXT(0)], vec2(max(dot(N, V), 0.0), roughness)).rg, 0.0, 1.0);
}