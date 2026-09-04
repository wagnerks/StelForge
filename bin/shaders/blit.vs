#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec2 vUV;

void main() {
    const vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );
    const vec2 uvs[3] = vec2[](
        vec2(0.0, 1.0),
        vec2(2.0, 1.0),
        vec2(0.0, -1.0)
    );
    vUV = uvs[SFE_VERTEX_ID];
    gl_Position = vec4(positions[SFE_VERTEX_ID], 0.0, 1.0);
}
