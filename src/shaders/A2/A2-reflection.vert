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

layout(location=0) out vec3 position;
layout(location=1) out vec3 normal;
layout(location=2) out vec2 texCoord;
layout(location=3) out float reflective;

void main() {
	gl_Position = PERSPECTIVE * VIEW * TRANSFORMS[gl_InstanceIndex].MODEL * vec4(Position, 1.0);
	
	position = mat4x3(TRANSFORMS[gl_InstanceIndex].MODEL) * vec4(Position, 1.0);
	normal = mat3(TRANSFORMS[gl_InstanceIndex].MODEL_NORMAL) * Normal;
	texCoord = TexCoord;

	reflective = TRANSFORMS[gl_InstanceIndex].MODEL_NORMAL[3][3];

	// if(TRANSFORMS[gl_InstanceIndex].MODEL_NORMAL[3][3] == 1.0){
	// 	camera_view = CAMERA_POSITION.xyz - position;
	// }
	// else{
	// 	camera_view = normal;
	// }
}