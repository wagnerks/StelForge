#version 450 core
#include "sfe.glsl"
// Declared only to match cascadeShadowMap_csm.vs, which it shares with the cutout shader.
layout(location = 3) flat in uint vMaterial;
layout(location = 4) flat in float vPiece;

// Depth-only casters. Deliberately empty: any discard in a fragment shader forces the whole
// pipeline off early-Z, so opaque geometry must not share the cutout shader.
void main() {
}
