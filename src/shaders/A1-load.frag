#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set=2,binding=0) uniform sampler2D TEXTURE[];

layout(push_constant) uniform Push {
    uint MATERIAL_INDEX;
} push;

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texCoord;

layout(location=0) out vec4 outColor;

void main() {
	vec3 n = normalize(normal);
	vec3 l = vec3(0.3, 0.4, 0.5);
	vec3 albedo = texture(TEXTURE[nonuniformEXT(push.MATERIAL_INDEX)], texCoord).rgb;

	//hemisphere lighting from direction l:
	vec3 e = vec3(0.5 * dot(n,l) + 0.5);
	// vec3 e = SKY_ENERGY * (0.5 * dot(n,SKY_DIRECTION) + 0.5)
	//        + SUN_ENERGY * max(0.0, dot(n,SUN_DIRECTION)) ;

	// outColor = vec4(e * albedo, 1.0);
	outColor = vec4(1.0, 1.0, 1.0, 1.0);
}