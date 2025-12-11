#version 450

layout(location=0) in vec3 Position;
layout(location=1) in vec3 Normal;
layout(location=2) in vec2 Tangent;
layout(location=3) in vec2 TexCoord;
struct Transform {
	mat4 PERSPECTIVE;
	mat4 VIEW;
	mat4 MODEL;
	mat4 MODEL_NORMAL;
};

layout(set=1, binding=0, std140) readonly buffer Transforms {
	Transform TRANSFORMS[];
};

layout(location=0) out vec3 position;
layout(location=1) out vec3 normal;
layout(location=2) out vec2 texCoord;

void main() {
	gl_Position = TRANSFORMS[gl_InstanceIndex].PERSPECTIVE * TRANSFORMS[gl_InstanceIndex].VIEW * TRANSFORMS[gl_InstanceIndex].MODEL * vec4(Position, 1.0);
	position = mat4x3(TRANSFORMS[gl_InstanceIndex].MODEL) * vec4(Position, 1.0);
	normal = mat3(TRANSFORMS[gl_InstanceIndex].MODEL_NORMAL) * Normal;
	texCoord = TexCoord;
}