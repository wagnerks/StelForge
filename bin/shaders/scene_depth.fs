#version 450 core
#include "sfe.glsl"
// SceneDepthPass: the depth buffer as a colour texture, 1 where nothing was drawn.
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D depthBuffer;

void main() {
    FragColor = vec4(texelFetch(depthBuffer, ivec2(gl_FragCoord.xy), 0).r, 0.0, 0.0, 1.0);
}
