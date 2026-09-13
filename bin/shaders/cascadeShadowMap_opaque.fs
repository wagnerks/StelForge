#version 450 core
#include "sfe.glsl"

// Depth-only casters. Deliberately empty: any discard in a fragment shader forces the whole
// pipeline off early-Z, so opaque geometry must not share the cutout shader.
void main() {
}
