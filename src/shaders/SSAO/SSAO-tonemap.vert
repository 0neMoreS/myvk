#version 450

void main() {
    // Generate full-screen triangle using vertex index
    // Vertex 0: (-1, -1) -> UV (0, 0)
    // Vertex 1: ( 3, -1) -> UV (2, 0)
    // Vertex 2: (-1,  3) -> UV (0, 2)
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
