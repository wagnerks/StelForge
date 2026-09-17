#version 450 core
// A soft round speck -- a mote, a gnat, a firefly -- or, where the wings are open, a butterfly: its light added, and
// as much of the picture behind it as it covers taken away.
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec3 vLight;
layout(location = 2) in float vCover;
layout(location = 3) in vec2 vShape;

layout(location = 0) out vec4 FragColor;

void main() {
    float shape;
    if (vShape.x > 0.5) {
        // Two wings, their span as open as the beat leaves them, and a body down the middle.
        vec2 p = vec2(vUv.x / max(vShape.y, 0.1), vUv.y);
        // A wing each side of the body, the pair of them wider than they are tall.
        float wings = 1.0 - smoothstep(0.3, 0.62, length(vec2(abs(p.x) - 0.5, p.y * 1.35)));
        float body = 1.0 - smoothstep(0.06, 0.16, abs(vUv.x) * max(vShape.y, 0.1)) * step(0.55, abs(vUv.y));
        shape = clamp(wings * body, 0.0, 1.0);
    }
    else {
        shape = 1.0 - smoothstep(0.35, 1.0, length(vUv));
    }
    FragColor = vec4(vLight * shape, vCover * shape);
}
