#version 450 core
#include "sfe.glsl"
#include "sfe_post.glsl"
// What the eye is used to, one texel carried from frame to frame: r the log luminance it has adapted to, easing
// towards the frame's; g how much of the sun is in sight, for the lens flare, eased as well so a sun passing behind
// leaves does not flicker it.
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D frameLuminance; // 1x1, log
layout(SFE_SET(0, 1)) uniform sampler2D previous;       // 1x1, this shader's last output
layout(SFE_SET(0, 2)) uniform sampler2D sceneDepth;  // 1 where the sky is

void main() {
    vec4 before = texture(previous, vec2(0.5));
    float current = texture(frameLuminance, vec2(0.5)).r;
    float follow = 1.0 - exp(-params.adaptSeconds * params.adaptSpeed);
    float adapted = mix(before.r, current, clamp(follow, 0.0, 1.0));

    float visible = 0.0;
    vec2 sun = vec2(params.sunU, params.sunV);
    if (params.sunInFront > 0.5 && all(greaterThan(sun, vec2(0.0))) && all(lessThan(sun, vec2(1.0)))) {
        const int kTaps = 12;
        for (int i = 0; i < kTaps; ++i) {
            float angle = float(i) / float(kTaps) * 6.2831853;
            float radius = (i % 2 == 0) ? 0.004 : 0.009;
            vec2 at = sun + vec2(cos(angle) / params.aspect, sin(angle)) * radius;
            visible += texture(sceneDepth, at).r >= 1.0 ? 1.0 : 0.0;
        }
        visible /= float(kTaps);
    }
    float seen = mix(before.g, visible, clamp(1.0 - exp(-params.adaptSeconds * 12.0), 0.0, 1.0));
    FragColor = vec4(adapted, seen, 0.0, 1.0);
}
