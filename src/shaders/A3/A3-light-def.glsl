struct SunLight {
	float cascadeSplits[4];
	mat4 orthographic[4]; // Project points in world space to texture uv
	vec3 direction; // Object to light, in world space
	float angle;
	vec3 tint; // Already multiplied by strength
	int shadow; // Shadow map size
};

struct SphereLight {
	vec3 position; // In world space
	float radius;
	vec3 tint; // Already multiplied by power
	float limit;
};

struct SpotLight {
	mat4 perspective; // Project points in world space to texture uv
	vec3 position; // In world space
	float radius;
	vec3 direction; // Object to light, in world space
	float fov;
	vec3 tint; // Already multiplied by power
	float blend;
	float limit;
	int shadow; // Shadow map size
};

layout(set=0, binding=1, std430) readonly buffer SunLightsBuf {
    uint count;
    SunLight lights[];
} sunLightsBuf;

layout(set=0, binding=2, std430) readonly buffer SphereLightsBuf {
    uint count;
    SphereLight lights[];
} sphereLightsBuf;

layout(set=0, binding=3, std430) readonly buffer SpotLightsBuf {
    uint count;
    SpotLight lights[];
} spotLightsBuf;

layout(set=0, binding=4, std430) readonly buffer ShadowSunLightsBuf {
    uint count;
    SunLight shadowLights[];
} shadowSunLightsBuf;

layout(set=0, binding=5, std430) readonly buffer ShadowSphereLightsBuf {
    uint count;
    SphereLight shadowLights[];
} shadowSphereLightsBuf;

layout(set=0, binding=6, std430) readonly buffer ShadowSpotLightsBuf {
    uint count;
    SpotLight shadowLights[];
} shadowSpotLightsBuf;

layout(set=2,binding=2) uniform sampler2DArray sunShadowMap[];
layout(set=2,binding=3) uniform samplerCube sphereShadowMap[];
layout(set=2,binding=4) uniform sampler2D spotShadowMap[];