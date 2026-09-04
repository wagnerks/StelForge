#version 450 core
#include "sfe.glsl"
layout(location = 0) in vec2 vertex;
layout(location = 0) out vec2 TexCoords;

layout(SFE_SET(0, 1), std140) uniform DrawData {
    mat4 projection;
} dd;

void main()
{
    TexCoords = vertex * 0.5 + 0.5;
    gl_Position = vec4(vertex.xy, 0.0, 1.0);
}
