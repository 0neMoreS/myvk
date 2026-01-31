#version 450

layout(location=0) in vec3 Position;
layout(location=1) in vec3 Normal;
layout(location=2) in vec4 Tangent;
layout(location=3) in vec2 TexCoord;

struct Transform {
	mat4 MODEL;
	mat4 MODEL_NORMAL;
};

layout(set=0,binding=0,std140) uniform PV {
    mat4 PERSPECTIVE;
    mat4 VIEW;
	vec4 CAMERA_POSITION;
	vec4 LIGHT_POSITION;
};

layout(set=1, binding=0, std430) readonly buffer Transforms {
	Transform TRANSFORMS[];
};

layout(location=0) out vec3 fragPos;
layout(location=1) out vec3 normal;
layout(location=2) out vec2 texCoord;
layout(location=3) out vec3 tangentLightPos;
layout(location=4) out vec3 tangentCameraPos;
layout(location=5) out vec3 tangentFragPos;

void main() {
	fragPos = mat4x3(TRANSFORMS[gl_InstanceIndex].MODEL) * vec4(Position, 1.0);
	normal = normalize(vec3(TRANSFORMS[gl_InstanceIndex].MODEL_NORMAL * vec4(Normal, 0.0)));
	
	vec3 T = normalize(vec3(TRANSFORMS[gl_InstanceIndex].MODEL_NORMAL * vec4(Tangent.xyz, 0.0)));
	float tangentSign = Tangent.w;
	vec3 N = normal;
	vec3 B = cross(N, T) * tangentSign;
	mat3 TBN = transpose(mat3(T, B, N));

	tangentLightPos = TBN * LIGHT_POSITION.xyz;
	tangentCameraPos = TBN * CAMERA_POSITION.xyz;
	tangentFragPos = TBN * fragPos;
	texCoord = TexCoord;

	gl_Position = PERSPECTIVE * VIEW * TRANSFORMS[gl_InstanceIndex].MODEL * vec4(Position, 1.0);
}