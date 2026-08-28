#version 460 core

layout(triangles, invocations = 6) in;
layout(triangle_strip, max_vertices = 36) out;

layout (std140, binding = 3) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[72];
};

layout(std140, binding = 15) uniform PushConstants {
    int lightCount;
    float casterOpacity;
} pc;

void main()
{
    int count = min(pc.lightCount, 12);
    for (int light = 0; light < count; ++light)
    {
        int layer = light * 6 + gl_InvocationID;
        for (int i = 0; i < 3; ++i)
        {
            gl_Position = lightSpaceMatrices[layer] * gl_in[i].gl_Position;
            gl_Layer = layer;
            EmitVertex();
        }
        EndPrimitive();
    }
}
