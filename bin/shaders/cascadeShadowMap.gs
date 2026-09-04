#version 450 core
#include "sfe.glsl"
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(SFE_SET(0, 11), std140) uniform LightSpaceMatrices {
    mat4 lightSpaceMatrices[6];
};

SFE_PUSH_BLOCK PushConstants {
    float casterOpacity;
    int cascadeLayer;
} params;

void main() {
    int layer = clamp(params.cascadeLayer, 0, 5);
    for (int i = 0; i < 3; ++i) {
        vec4 clip = lightSpaceMatrices[layer] * gl_in[i].gl_Position;
        clip.z = (clip.z + clip.w) * 0.5;
        gl_Position = clip;
        gl_Layer = layer;
        EmitVertex();
    }
    EndPrimitive();
}
