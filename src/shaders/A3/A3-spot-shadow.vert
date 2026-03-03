#version 450

layout(location=0) in vec3 Position;
layout(location=1) in vec3 Normal;
layout(location=2) in vec4 Tangent;
layout(location=3) in vec2 TexCoord;

struct Transform {
    mat4 MODEL;
    mat4 MODEL_NORMAL;
};

struct SpotLight {
    mat4 perspective;
    vec3 position;
    float radius;
    vec3 direction;
    float fov;
    vec3 tint;
    float blend;
    float limit;
    int shadow;
};

layout(set=0, binding=0, std430) readonly buffer ShadowSpotLightsBuf {
    uint count;
    SpotLight shadowLights[];
} shadowSpotLightsBuf;

layout(set=1, binding=0, std430) readonly buffer Transforms {
    Transform TRANSFORMS[];
};

layout(push_constant) uniform Push {
    uint LIGHT_INDEX;
} push;

void main() {
    vec3 world_pos = mat4x3(TRANSFORMS[gl_InstanceIndex].MODEL) * vec4(Position, 1.0);
    gl_Position = shadowSpotLightsBuf.shadowLights[push.LIGHT_INDEX].perspective * vec4(world_pos, 1.0);
}
