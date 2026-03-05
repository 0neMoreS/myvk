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
	outColor = vec4(debugSunLightShadow(shadowSunLightsBuf.shadowLights[0], position, viewPosition, sunShadowMap[0]), 1.0);
}