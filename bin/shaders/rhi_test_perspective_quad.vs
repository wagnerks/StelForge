#version 450 core
#include "sfe.glsl"

// A floor quad seen through a real perspective projection, straight from the vertex index: world
// y = -1 under a camera at the origin looking down -z, yawed so both uv axes change along both
// screen axes. The uv then has derivatives that differ per pixel, from magnified near the camera to
// strongly minified far away, which is what an implicit-LOD lookup on a textured surface sees.
// Clip z is half of w, inside both OpenGL's [-w, w] and Vulkan's [0, w].
layout(location = 0) out vec2 vUv;

void main() {
    vec2 corners[6] = vec2[6](vec2(-3.0, -0.6), vec2(3.0, -0.6), vec2(3.0, -14.0),
        vec2(-3.0, -0.6), vec2(3.0, -14.0), vec2(-3.0, -14.0));
    vec2 xz = corners[SFE_VERTEX_ID];
    const float yaw = 0.35;
    vec3 view = vec3(xz.x * cos(yaw) - xz.y * sin(yaw), -1.0, xz.x * sin(yaw) + xz.y * cos(yaw));
    const float f = 1.7320508; // 1 / tan(30 degrees)
    float w = -view.z;
    vUv = vec2(xz.x, -xz.y) * 0.5;
    gl_Position = vec4(f * view.x, f * view.y, 0.5 * w, w);
}
