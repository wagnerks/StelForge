#version 450 core
#include "sfe.glsl"
layout(triangles, invocations = 6) in;
layout(triangle_strip, max_vertices = 36) out;

layout(SFE_SET(0, 11), std140) uniform LightSpaceMatrices {
    mat4 lightSpaceMatrices[72];
};

SFE_PUSH_BLOCK Push {
    int lightCount;
    float casterOpacity;
} pc;

void main() {
    int count = min(pc.lightCount, 12);
    for (int light = 0; light < count; ++light) {
        int layer = light * 6 + gl_InvocationID;
        for (int i = 0; i < 3; ++i) {
            gl_Position = lightSpaceMatrices[layer] * gl_in[i].gl_Position;
            gl_Layer = layer;
            EmitVertex();
        }
        EndPrimitive();
    }
}
