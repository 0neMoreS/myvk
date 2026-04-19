#version 450

layout(location=0) in vec3 Position;
layout(location=1) in vec4 Color;

layout(set=0,binding=0,std140) uniform PV {
    mat4 PERSPECTIVE;
	mat4 INV_PERSPECTIVE;
    mat4 VIEW;
	mat4 INV_PV;
	vec4 CAMERA_POSITION;
};

layout(location=0) out vec4 color;

void main() {
	color = Color;
	gl_Position = PERSPECTIVE * VIEW * vec4(Position, 1.0);
}