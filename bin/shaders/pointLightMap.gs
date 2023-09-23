#version 460 core
    
layout(triangles, invocations = 6) in;
layout(triangle_strip, max_vertices = 3) out;

layout (std140, binding = 3) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[36];
};

uniform int offset = 0; 

void main()
{          
    for (int i = 0; i < 6; ++i)
    {
        gl_Position = lightSpaceMatrices[gl_InvocationID + offset] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID + offset;
        EmitVertex();
    }
    EndPrimitive();
}  