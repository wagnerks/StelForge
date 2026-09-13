#version 450 core
#include "sfe.glsl"

// The inputs cascadeShadowMap_csm.vs declares, at the same locations, over the same Vertex3D layout.
// Both texture coordinate sets go straight into the colour: texCoords1 in red and green, texCoords in
// blue and alpha, so one pixel says which bytes each location was fed.
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aTexCoords;
layout(location = 5) in ivec4 aBoneIds;
layout(location = 6) in vec4 aWeights;
layout(location = 7) in vec2 aTexCoords1;

layout(location = 0) flat out vec4 vColor;

void main() {
    vColor = vec4(aTexCoords1, aTexCoords);
    // Bone ids and weights take no part in the colour, but they feed depth (unused, depth test is
    // off), so the linker keeps both inputs active exactly as in the shadow shader.
    gl_Position = vec4(aPos.xy, float(aBoneIds[0]) * aWeights[0] * 0.01, 1.0);
    gl_PointSize = 1.0;
}
