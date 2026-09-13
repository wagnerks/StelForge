#version 450 core
#include "sfe.glsl"

// The opposite of rhi_test_triangle.vs: nothing here comes from the vertex index, so a draw only
// lands where the bound vertex buffer says. The colour rides along in the same buffer at a
// non-zero attribute offset, which is what makes the stride and offset in VertexLayoutDesc load
// bearing rather than decorative.
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec4 inColor;

// flat: all three vertices carry the same colour, so interpolation would only add a rounding
// question to a test that is about positions.
layout(location = 0) flat out vec4 vColor;

void main() {
    vColor = inColor;
    gl_Position = vec4(inPos, 0.0, 1.0);
    // Only read when the pipeline's topology is Points, and then it has to be written: Vulkan leaves
    // the size of a point undefined if the vertex stage never sets it, so a topology test would be
    // asserting on whatever the driver happened to leave in the register. OpenGL ignores this
    // entirely unless GL_PROGRAM_POINT_SIZE is enabled, which the backend never does, so its points
    // are one pixel too and the two backends are held to the same size on purpose.
    gl_PointSize = 1.0;
}
