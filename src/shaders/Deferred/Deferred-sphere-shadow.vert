#version 450

layout(location=0) in vec3 Position;
layout(location=1) in vec3 Normal;
layout(location=2) in vec4 Tangent;
layout(location=3) in vec2 TexCoord;

struct Transform {
    mat4 MODEL;
    mat4 MODEL_NORMAL;
};

struct SphereLight {
    vec3 position;
    float radius;
    vec3 tint;
    float near_plane;
    float far_plane;
    int shadow;
	int _pad_[2];
};

struct SphereShadowMatrices {
    mat4 facePV[6];
};

layout(set=0, binding=0, std430) readonly buffer ShadowSphereLightsBuf {
    uint count;
    SphereLight shadowLights[];
} shadowSphereLightsBuf;

layout(set=0, binding=1, std430) readonly buffer ShadowSphereMatricesBuf {
    uint count;
    SphereShadowMatrices shadowMatrices[];
} shadowSphereMatricesBuf;

layout(set=1, binding=0, std430) readonly buffer Transforms {
    Transform TRANSFORMS[];
};

layout(push_constant) uniform Push {
    uint LIGHT_INDEX;
    uint FACE_INDEX;
} push;

void main() {
    vec3 world_pos = mat4x3(TRANSFORMS[gl_InstanceIndex].MODEL) * vec4(Position, 1.0);
    gl_Position = shadowSphereMatricesBuf.shadowMatrices[push.LIGHT_INDEX].facePV[push.FACE_INDEX] * vec4(world_pos, 1.0);
}
