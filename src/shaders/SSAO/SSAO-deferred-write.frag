#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set=2,binding=1) uniform sampler2D Textures[];

layout(push_constant) uniform Push {
    uint MATERIAL_INDEX;
} push;

layout(location=0) in vec3 fragPos;
layout(location=1) in vec2 texCoord;
layout(location=2) in vec3 viewFragPos;
layout(location=3) in mat3 TBN;

layout(location=0) out vec4 outGBufferAlbedo;
layout(location=1) out vec4 outGBufferNormal;
layout(location=2) out vec4 outGBufferPbr;

void main() {
    vec3 N = normalize(TBN * texture(Textures[nonuniformEXT(push.MATERIAL_INDEX)], texCoord).xyz * 2.0 - 1.0);
    vec3 albedo = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 2)], texCoord).xyz;
    float roughness = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 3)], texCoord).x;
    float metallic = texture(Textures[nonuniformEXT(push.MATERIAL_INDEX + 4)], texCoord).x;
    float ao = 1.0;

    // RT0: albedo.
    outGBufferAlbedo = vec4(albedo, 1.0);

    // RT1: world-space normal in float format.
    outGBufferNormal = vec4(N, 1.0);

    // RT2: metallic in .r, AO in .g, roughness in .a.
    outGBufferPbr = vec4(metallic, ao, 0.0, roughness);
}
