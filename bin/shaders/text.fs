#version 430 core
in vec2 TexCoords;
out vec4 color;

layout(binding = 24) uniform sampler2D text;

layout(std140, binding = 15) uniform PushConstants {
    vec3 textColor;
} pc;

void main()
{    
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(pc.textColor, 1.0) * sampled;
    //color = vec4(1.0)
}