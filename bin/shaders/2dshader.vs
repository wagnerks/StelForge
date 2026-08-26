#version 330 core
layout (location = 0) in vec2 vertex; // <vec2 pos, vec2 tex>
out vec2 TexCoords;

layout (std140, binding = 1) uniform DrawData {
    mat4 projection;
} dd;

void main()
{
    //gl_Position = dd.projection * vec4(vertex.xy, 0.0, 1.0);
    gl_Position = vec4(vertex.xy, 0.0, 1.0);
}