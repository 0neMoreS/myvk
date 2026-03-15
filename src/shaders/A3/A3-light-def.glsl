// Compile-time switch: comment out to disable Tiled Forward Lighting.
// Must match #define USE_TILED_LIGHTING in LightsManager.hpp.
#define USE_TILED_LIGHTING

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
	float near_plane;
	float far_plane;
	int shadow; // Shadow map size
	int _pad_[2];
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
	float near_plane;
	float far_plane;
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

#ifdef USE_TILED_LIGHTING
// Per-tile light list entry: start offset + count in the flat index buffer.
struct TileInfo {
    uint offset; // index into indices[]
    uint count;  // number of lights touching this tile
};

// Sphere lights: tile→index mapping (set=0, binding=7,8)
layout(set=0, binding=7, std430) readonly buffer SphereTileDataBuf {
    uint tiles_x;
    uint tiles_y;
    TileInfo tiles[]; // [tiles_y * tiles_x]
} sphereTileDataBuf;

layout(set=0, binding=8, std430) readonly buffer SphereLightIdxBuf {
    uint indices[];
} sphereLightIdxBuf;

// Spot lights: tile→index mapping (set=0, binding=9,10)
layout(set=0, binding=9, std430) readonly buffer SpotTileDataBuf {
    uint tiles_x;
    uint tiles_y;
    TileInfo tiles[]; // [tiles_y * tiles_x]
} spotTileDataBuf;

layout(set=0, binding=10, std430) readonly buffer SpotLightIdxBuf {
    uint indices[];
} spotLightIdxBuf;

// Shadow sphere lights: tile→index mapping (set=0, binding=11,12)
layout(set=0, binding=11, std430) readonly buffer ShadowSphereTileDataBuf {
	uint tiles_x;
	uint tiles_y;
	TileInfo tiles[]; // [tiles_y * tiles_x]
} shadowSphereTileDataBuf;

layout(set=0, binding=12, std430) readonly buffer ShadowSphereLightIdxBuf {
	uint indices[];
} shadowSphereLightIdxBuf;

// Shadow spot lights: tile→index mapping (set=0, binding=13,14)
layout(set=0, binding=13, std430) readonly buffer ShadowSpotTileDataBuf {
	uint tiles_x;
	uint tiles_y;
	TileInfo tiles[]; // [tiles_y * tiles_x]
} shadowSpotTileDataBuf;

layout(set=0, binding=14, std430) readonly buffer ShadowSpotLightIdxBuf {
	uint indices[];
} shadowSpotLightIdxBuf;
#endif // USE_TILED_LIGHTING

layout(set=2,binding=2) uniform sampler2DArray sunShadowMap[];
layout(set=2,binding=3) uniform samplerCube sphereShadowMap[];
layout(set=2,binding=4) uniform sampler2D spotShadowMap[];