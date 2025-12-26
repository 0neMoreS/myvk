#version 450

layout(set=1,binding=0,std140) uniform World {
	vec3 SKY_DIRECTION;
	vec3 SKY_ENERGY; //energy supplied by sky to a surface patch with normal = SKY_DIRECTION
	vec3 SUN_DIRECTION;
	vec3 SUN_ENERGY; //energy supplied by sun to a surface patch with normal = SUN_DIRECTION
};

layout(set=3,binding=0) uniform sampler2D TEXTURE;
layout(set=4,binding=0) uniform samplerCube CUBEMAP;

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texCoord;

layout(location=0) out vec4 outColor;

void main() {
	// Sample cubemap for indirect lighting
	vec3 cubemap_color = texture(CUBEMAP, position).rgb;
	outColor = vec4(cubemap_color, 1.0);
}