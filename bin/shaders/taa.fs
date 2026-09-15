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

// The clamp works in YCoCg because the interesting axis is brightness: a neighbourhood box in RGB is a
// cube around three correlated channels and ends up far larger than the colours actually present, so a
// stale history survives inside it. Separating luma from two chroma axes makes the box tight where it
// matters and loose where the eye does not look.
vec3 rgbToYCoCg(vec3 c) {
    return vec3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b,
                0.5 * c.r - 0.5 * c.b,
               -0.25 * c.r + 0.5 * c.g - 0.25 * c.b);
}

vec3 yCoCgToRgb(vec3 c) {
    return vec3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z);
}

// Pulls the history to the edge of the box along the line towards the neighbourhood mean, instead of
// clamping each channel on its own. A per-channel clamp invents a colour that is in none of the taps.
vec3 clipToBox(vec3 history, vec3 boxMin, vec3 boxMax) {
    vec3 centre = 0.5 * (boxMax + boxMin);
    vec3 extent = 0.5 * (boxMax - boxMin) + 1e-5;
    vec3 offset = history - centre;
    vec3 units = abs(offset / extent);
    float worst = max(units.x, max(units.y, units.z));
    return worst > 1.0 ? centre + offset / worst : history;
}

// Bilinear resampling of the history loses a little sharpness every frame, and after a few dozen frames
// of a moving camera the accumulated image is visibly softer than the one being fed in. Catmull-Rom
// costs a handful of taps and keeps it.
vec3 sampleHistory(vec2 uv, vec2 texSize) {
    vec2 samplePos = uv * texSize;
    vec2 centre = floor(samplePos - 0.5) + 0.5;
    vec2 f = samplePos - centre;

    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);

    // The middle two taps are folded into one bilinear fetch placed between them, which is what makes
    // this nine weights but five lookups.
    vec2 w12 = w1 + w2;
    vec2 uv0 = (centre - 1.0) / texSize;
    vec2 uv12 = (centre + w2 / w12) / texSize;
    vec2 uv3 = (centre + 2.0) / texSize;

    vec3 result = vec3(0.0);
    float weight = 0.0;
    result += texture(historyColor, vec2(uv12.x, uv0.y)).rgb * (w12.x * w0.y);
    weight += w12.x * w0.y;
    result += texture(historyColor, vec2(uv0.x, uv12.y)).rgb * (w0.x * w12.y);
    weight += w0.x * w12.y;
    result += texture(historyColor, vec2(uv12.x, uv12.y)).rgb * (w12.x * w12.y);
    weight += w12.x * w12.y;
    result += texture(historyColor, vec2(uv3.x, uv12.y)).rgb * (w3.x * w12.y);
    weight += w3.x * w12.y;
    result += texture(historyColor, vec2(uv12.x, uv3.y)).rgb * (w12.x * w3.y);
    weight += w12.x * w3.y;
    // The kernel has negative lobes, so it can undershoot into black on a hard edge; the box clip below
    // catches the rest, but nothing here may go negative before it.
    return max(result / max(weight, 1e-5), vec3(0.0));
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(sceneHdr, 0));
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

    // Mean and deviation of the neighbourhood rather than its extremes: one bright sub-pixel sample --
    // a thin sword edge, a specular glint -- used to stretch the box far enough to let a whole frame of
    // aliasing through, which is what made distant characters crawl.
    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec3 c = rgbToYCoCg(texture(sceneHdr, TexCoords + vec2(x, y) * texel).rgb);
            m1 += c;
            m2 += c * c;
        }
    }
    const float kInvTaps = 1.0 / 9.0;
    vec3 mu = m1 * kInvTaps;
    vec3 sigma = sqrt(max(m2 * kInvTaps - mu * mu, vec3(0.0)));
    // 1.25 deviations: wide enough that a surface with real texture keeps its history, tight enough that
    // the frame behind a moving silhouette is rejected instead of ghosting.
    const float kGamma = 1.25;
    vec3 history = rgbToYCoCg(sampleHistory(prevUV, vec2(textureSize(sceneHdr, 0))));
    history = clipToBox(history, mu - kGamma * sigma, mu + kGamma * sigma);
    history = yCoCgToRgb(history);

    // A true velocity now, zero for a still camera, so it needs no slack for the jitter offset.
    float motion = length((TexCoords - prevUV) * vec2(textureSize(sceneHdr, 0)));
    // max() keeps the ramp pointing the right way: a blend at or above 0.72 would otherwise make
    // the two ends meet and collapse this into a constant, silently disabling the motion response.
    // Local contrast used to push this up as well, back when the history could only be reprojected by the
    // camera: it hid the ghosting on detailed models, at the price of keeping 28% of every jittered frame
    // on them forever, which is a visible crawl. The motion vectors do that job properly now.
    float restW = 1.0 - params.blend;
    float currentW = mix(restW, max(restW, 0.28), clamp(motion * 0.08, 0.0, 1.0));

    // Weighted by inverse luminance, so a bright pixel counts for less than a dark one in the average.
    // A firefly -- one sample that caught a highlight this frame -- is what the eye reads as sparkle, and
    // this is what stops it from dragging the result up and then letting it fall back next frame.
    float currentWeight = currentW / (1.0 + sfeLuma(current));
    float historyWeight = (1.0 - currentW) / (1.0 + sfeLuma(history));
    vec3 color = (current * currentWeight + history * historyWeight) / max(currentWeight + historyWeight, 1e-5);
    FragColor = vec4(color, 1.0);
}
