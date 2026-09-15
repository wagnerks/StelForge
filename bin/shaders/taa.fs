#version 450 core
#include "sfe.glsl"
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(SFE_SET(0, 0)) uniform sampler2D sceneHdr;
layout(SFE_SET(0, 1)) uniform sampler2D historyColor;
layout(SFE_SET(0, 2)) uniform sampler2D worldPos;
// Where each pixel was last frame, written by the G-buffer from the previous pose and transform.
layout(SFE_SET(0, 3)) uniform sampler2D velocity;

layout(SFE_SET(0, 15), std140) uniform PushConstants {
    mat4 prevPV;
    mat4 curPV;
    vec2 resolution;
    float blend;
    int reset;
} params;

vec3 neighborhoodClamp(vec3 history, vec2 texel) {
    vec3 aabbMin = vec3(1e10);
    vec3 aabbMax = vec3(-1e10);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec3 c = texture(sceneHdr, TexCoords + vec2(x, y) * texel).rgb;
            aabbMin = min(aabbMin, c);
            aabbMax = max(aabbMax, c);
        }
    }
    return clamp(history, aabbMin, aabbMax);
}

void main() {
    vec3 current = texture(sceneHdr, TexCoords).rgb;
    if (params.reset != 0) {
        FragColor = vec4(current, 1.0);
        return;
    }

    // The G-buffer's motion vector: it follows the surface itself, so a skinned character's history comes from
    // where that pixel of it was, not from the background it moved over -- which is what smeared a trail behind
    // every animated model. Both clip positions behind it are unjittered, so a still surface gives exactly zero.
    // Where nothing was drawn (the sky) the velocity is zero and the camera difference on the world position
    // still applies.
    vec3 world = texture(worldPos, TexCoords).rgb;
    vec2 prevUV = TexCoords;
    if (dot(world, world) > 1e-4) {
        prevUV = TexCoords - texture(velocity, TexCoords).xy;
    }

    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        FragColor = vec4(current, 1.0);
        return;
    }

    vec2 texel = 1.0 / vec2(textureSize(sceneHdr, 0));
    vec3 history = neighborhoodClamp(texture(historyColor, prevUV).rgb, texel);
    // A true velocity now, zero for a still camera, so it needs no slack for the jitter offset.
    float motion = length((TexCoords - prevUV) * vec2(textureSize(sceneHdr, 0)));
    // max() keeps the ramp pointing the right way: a blend at or above 0.72 would otherwise make
    // the two ends meet and collapse this into a constant, silently disabling the motion response.
    // Local contrast used to push this up as well, back when the history could only be reprojected by the
    // camera: it hid the ghosting on detailed models, at the price of keeping 28% of every jittered frame
    // on them forever, which is a visible crawl. The motion vectors do that job properly now.
    float restW = 1.0 - params.blend;
    float currentW = mix(restW, max(restW, 0.28), clamp(motion * 0.08, 0.0, 1.0));
    vec3 color = mix(history, current, currentW);
    FragColor = vec4(color, 1.0);
}
