#version 450

layout(location=0) in vec3 Position;
layout(location=1) in vec3 Normal;
layout(location=2) in vec4 Tangent;
layout(location=3) in vec2 TexCoord;

layout(location = 0) out vec3 normal;

layout(set=0,binding=0,std140) uniform PV {
    mat4 PERSPECTIVE;
    mat4 VIEW;
    vec4 CAMERA_POSITION;
};

void main() {
    // Remove translation so the cube stays centered on the camera
    normal = Normal;

    gl_Position = PERSPECTIVE * mat4(mat3(VIEW))* vec4(Position, 1.0);
}
