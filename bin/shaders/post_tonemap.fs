#version 450 core
#include "sfe.glsl"
#include "sfe_post.glsl"
// Every pixel of the frame through the tonemap, once, for post.fs to sharpen with.
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

#include "sfe_tonemap.glsl"

void main() {
    FragColor = vec4(tonemapped(TexCoords, sfeToneGain()), 1.0);
}
