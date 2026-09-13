#version 450 core
#include "sfe.glsl"

// An Int4 attribute next to a float one, the way Vertex3D carries bone ids. Every vertex stores
// -1 in all four ids; anything else means the integer attribute was not fetched from the buffer,
// and the vertex collapses to w = 0 so the draw writes nothing instead of writing somewhere wrong.
layout(location = 0) in vec2 inPos;
layout(location = 1) in ivec4 inIds;

void main() {
    gl_Position = inIds == ivec4(-1) ? vec4(inPos, 0.0, 1.0) : vec4(0.0);
}
