#version 450 core
#include "sfe.glsl"
#include "sfe_material_table.glsl"
#include "sfe_wind.glsl"
#include "sfe_terrain.glsl"
#include "sfe_grass.glsl"
// One plant of a chunk's grid: a reed (now and then a cattail) or a lily pad. Everything about it -- where exactly it
// stands, how tall, which way it faces and curls, its shade -- comes from its grid cell's hash, so the same cell grows
// the same plant every frame wherever the camera is. The wind bends the reeds in the same gusts the trees sway in.
layout(location = 0) in vec2 aPos;       // see GrassPass's plant meshes
layout(location = 1) in vec2 aTexCoords;

layout(location = 0) out vec3 vWorld;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vColor;
layout(location = 3) out vec4 vClip;
layout(location = 4) out vec4 vPrevClip;
layout(location = 5) out vec3 vPad; // lily pads: xy across the pad in -1..1, z 1 with a flower

layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

// Where each kind grows (r grass, b reeds, a lily pads) and the broad shade (g); see grass_map.fs.
layout(SFE_SET(0, 18)) uniform sampler2D grassMap;
// How deep in the forest; see sfeForest.
layout(SFE_SET(0, 19)) uniform sampler2D forestMap;

// Mirrors GrassPass::DrawParams.
SFE_PUSH_BLOCK GrassDraw {
    float originX;
    float originZ;
    float spacing;
    float side;
    float width;
    float heightScale;
    float fadeInStart;
    float fadeInEnd;
    float fadeOutStart;
    float fadeOutEnd;
    float tier;
    float kind; // 1 reed leaves, 2 lily pads, 3 cattails
} draw;

const int kReeds = 1;
const int kLilies = 2;
const int kCattails = 3;

float random01(ivec2 cell, uint salt) {
    return float(sfeCellHash(cell + ivec2(int(salt) * 7919, int(salt) * 104729)) >> 8u) / 16777216.0;
}

vec3 bezier(vec3 p0, vec3 p1, vec3 p2, float t) {
    float u = 1.0 - t;
    return u * u * p0 + 2.0 * u * t * p1 + t * t * p2;
}

// Where the tip ends up for this lean, keeping the blade about its length.
vec3 tipOffset(vec2 lean, float height) {
    float leanLength = min(length(lean), 1.25);
    vec2 direction = leanLength > 1e-4 ? lean / length(lean) : vec2(0.0);
    float horizontal = sin(leanLength) * height;
    float vertical = cos(leanLength) * height;
    return vec3(direction.x * horizontal, vertical, direction.y * horizontal);
}

vec2 windLean(vec2 root, vec4 wind, float phase) {
    float gust = sfeWindGust(root, wind);
    return wind.xz * wind.y * (gust * 0.9 + 0.25 * sin(wind.w * 2.4 + phase));
}

const float kReedStem = 10.0;
const float kReedHead = 11.0;
const float kReedSpike = 12.0;

// One vertex of a reed plant (GrassPass::appendReedPlant): a clump of long flat leaves fanning out of the water,
// bending over towards their tips and twisting as they go, and on some plants a stem carrying a cattail's brown head
// with a thin spike above it. Everything about the plant comes from its cell, as for a grass blade. `bendHeight` comes
// back as how far this vertex's sideways wind sway reaches, for the motion vector.
vec3 reedVertex(ivec2 cell, uint tierSalt, vec2 root, float ground, float size, float phase, int part, float along, float side,
    vec4 wind, out vec3 normal, out vec3 color, out float bendHeight) {
    // A cattail on most cells of its own grid: the rest leave its parts out before doing anything else.
    if (part >= 10 && random01(cell, tierSalt + 10u) > 0.6) {
        normal = vec3(0.0, 1.0, 0.0);
        color = vec3(0.0);
        bendHeight = 0.0;
        return vec3(0.0, -1e5, 0.0);
    }
    float aboveWater = max(-ground, 0.0);
    float plantHeight = (mix(1.0, 1.8, random01(cell, tierSalt + 4u)) + aboveWater) * size * draw.heightScale;
    vec3 base = vec3(root.x, grass.terrain.y + ground - 0.05, root.y);
    float tint = mix(0.85, 1.15, random01(cell, tierSalt + 9u));
    vec2 sway = windLean(root, wind, phase) * 0.35;
    vec3 up = vec3(0.0, 1.0, 0.0);

    if (part < 10) {
        uint leafSalt = tierSalt + 20u + uint(part) * 4u;
        // Spread round the plant by the golden angle, each a little off it.
        float yaw = random01(cell, tierSalt + 6u) * 6.2831853 + float(part) * 2.39996 + (random01(cell, leafSalt) - 0.5) * 0.9;
        vec2 out2 = vec2(cos(yaw), sin(yaw));
        vec3 outward = vec3(out2.x, 0.0, out2.y);
        float leafLength = plantHeight * mix(0.65, 1.0, random01(cell, leafSalt + 1u));
        float lean = mix(0.02, 0.22, random01(cell, leafSalt + 2u)) + 0.03 * float(part);
        float droop = mix(0.05, 0.45, random01(cell, leafSalt + 3u));
        // Leaning further the longer the leaf runs, its tip flutters on its own a little.
        vec2 bend = out2 * (lean + droop * 0.7) + sway * (1.0 + droop) + out2 * 0.04 * sin(wind.w * 3.1 + phase + float(part));
        vec3 tip = base + tipOffset(bend, leafLength);
        tip.y -= droop * droop * leafLength * 0.18;
        vec3 control = base + up * leafLength * 0.6 + outward * leafLength * lean * 0.25;
        vec3 spine = bezier(base, control, tip, along);
        vec3 tangent = normalize(2.0 * (1.0 - along) * (control - base) + 2.0 * along * (tip - control) + vec3(0.0, 1e-4, 0.0));
        // Flat face towards where it leans, turning over along its length.
        vec3 across = normalize(vec3(-out2.y, 0.0, out2.x));
        float twist = (random01(cell, leafSalt + 5u) - 0.5) * 2.4 * along;
        across = normalize(across * cos(twist) + cross(tangent, across) * sin(twist));
        float width = draw.width * mix(0.7, 1.3, random01(cell, leafSalt + 6u)) * mix(0.6, 1.0, size);
        float halfWidth = width * 0.5 * mix(0.55, 1.0, smoothstep(0.0, 0.25, along)) * (1.0 - pow(along, 2.5));
        // Folded along its midrib: the two halves catch the light a little differently.
        normal = normalize(normalize(cross(across, tangent)) + across * side * 0.35);
        vec3 leaf = mix(vec3(0.026, 0.040, 0.015), vec3(0.062, 0.078, 0.029), smoothstep(0.0, 0.6, along));
        bool dryTip = random01(cell, leafSalt + 7u) < 0.35;
        leaf = mix(leaf, dryTip ? vec3(0.110, 0.088, 0.040) : vec3(0.075, 0.088, 0.034), smoothstep(0.65, 1.0, along));
        color = leaf * tint;
        bendHeight = leafLength * along * along;
        return spine + across * side * halfWidth;
    }

    vec3 toCamera = grass.camera.xyz - base;
    toCamera.y = 0.0;
    toCamera = dot(toCamera, toCamera) > 1e-6 ? normalize(toCamera) : vec3(1.0, 0.0, 0.0);
    vec3 facingAcross = normalize(cross(up, toCamera));
    float stemHeight = plantHeight * mix(0.8, 1.02, random01(cell, tierSalt + 11u));
    vec2 stemLean = vec2(random01(cell, tierSalt + 12u) - 0.5, random01(cell, tierSalt + 13u) - 0.5) * 0.08 + sway * 0.7;
    vec3 stemTop = base + tipOffset(stemLean, stemHeight);
    vec3 stemDir = normalize(stemTop - base);
    float headLength = mix(0.14, 0.24, random01(cell, tierSalt + 14u)) * size;
    bendHeight = stemHeight * (part == int(kReedStem) ? along * along : 1.0);
    // Stem, head and spike face the camera, shaded as the round things they are.
    normal = normalize(toCamera * sqrt(max(1.0 - side * side, 0.0)) + facingAcross * side);
    if (part == int(kReedStem)) {
        color = vec3(0.032, 0.042, 0.016) * tint;
        return mix(base, stemTop, along) + facingAcross * side * 0.0045 * size;
    }
    if (part == int(kReedHead)) {
        // A cylinder with blunt, slightly narrower ends.
        float radius = 0.02 * size * mix(0.7, 1.0, 1.0 - pow(abs(2.0 * along - 1.0), 4.0));
        // Dark brown velvet: in the haze against the light a paler brown washes out to beige.
        color = vec3(0.026, 0.014, 0.007) * tint;
        return stemTop + stemDir * headLength * along + facingAcross * side * radius;
    }
    color = vec3(0.040, 0.034, 0.018) * tint;
    return stemTop + stemDir * (headLength + 0.12 * size * along) + facingAcross * side * 0.0025 * size * (1.0 - along);
}

void collapse() {
    gl_Position = vec4(0.0, 0.0, -2.0, 1.0);
    vWorld = vec3(0.0);
    vNormal = vec3(0.0, 1.0, 0.0);
    vColor = vec3(0.0);
    vClip = vec4(0.0);
    vPrevClip = vec4(0.0);
    vPad = vec3(0.0);
}

void main() {
    int kind = int(draw.kind + 0.5);
    int side = int(draw.side + 0.5);
    int instance = int(SFE_INSTANCE_ID);
    vec2 cellOrigin = vec2(draw.originX, draw.originZ) + vec2(instance % side, instance / side) * draw.spacing;
    ivec2 cell = ivec2(floor(cellOrigin / draw.spacing + 0.5));
    uint tierSalt = uint(draw.tier + 0.5) * 16u;

    vec2 root = cellOrigin + vec2(random01(cell, tierSalt + 1u), random01(cell, tierSalt + 2u)) * draw.spacing;
    vec2 local = root - grass.terrain.xz;
    float halfSize = grass.terrain.w * 0.5;
    if (any(greaterThan(abs(local), vec2(halfSize)))) {
        collapse();
        return;
    }

    // The cheap tests first: most cells of a chunk grow nothing, and every vertex of each finds that out on its own.
    float distanceToCamera = length(root - grass.camera.xz);
    float grow = draw.fadeInEnd > draw.fadeInStart ? smoothstep(draw.fadeInStart, draw.fadeInEnd, distanceToCamera) : 1.0;
    float shrink = 1.0 - smoothstep(draw.fadeOutStart, draw.fadeOutEnd, distanceToCamera);
    float size = grow * shrink;
    if (size < 0.02) {
        collapse();
        return;
    }
    vec4 map = textureLod(grassMap, (local + halfSize) / grass.terrain.w, 0.0);
    // Where the ground is mostly this kind's, all of it grows; towards the edges of its patch it thins out.
    float thinning = random01(cell, tierSalt + 3u);
    if (thinning > smoothstep(0.0, 0.55, kind == kLilies ? map.a : map.b)) {
        collapse();
        return;
    }
    float slope;
    float ground = sfeGrassGround(local, slope);

    float yaw = random01(cell, tierSalt + 6u) * 6.2831853;
    vec2 facing = vec2(cos(yaw), sin(yaw));
    vec3 across = vec3(-facing.y, 0.0, facing.x);
    float phase = random01(cell, tierSalt + 7u) * 6.2831853;

    if (kind == kReeds || kind == kCattails) {
        int part = int(aTexCoords.x + 0.5);
        vec3 normal;
        vec3 color;
        float bendHeight;
        vec3 world = reedVertex(cell, tierSalt, root, ground, size, phase, part, aPos.x, aPos.y, matrices.wind, normal, color, bendHeight);
        // Last frame's position: this one moved back by how much less the wind leant the plant then. Worked out
        // again from last frame's wind it doubled the cost of every reed vertex, for a motion vector off by a hair.
        vec2 swayChange = (windLean(root, matrices.wind, phase) - windLean(root, matrices.windBefore, phase)) * 0.35;
        vec3 worldBefore = world - vec3(swayChange.x, 0.0, swayChange.y) * bendHeight;
        vWorld = world;
        vNormal = normal;
        vColor = color;
        vPad = vec3(0.0);
        vClip = matrices.PVNoJitter * vec4(world, 1.0);
        vPrevClip = matrices.prevPVNoJitter * vec4(worldBefore, 1.0);
        gl_Position = matrices.PV * vec4(world, 1.0);
        return;
    }

    if (kind == kLilies) {
        // A pad floating on the water, drifting a little with the wind; pads that overlap sit at slightly different
        // heights. One in eight carries a flower (the flower mesh's parts, drawn on the same cells).
        int part = int(aTexCoords.x + 0.5);
        if (part >= 1 && random01(cell, tierSalt + 8u) >= 0.12) {
            collapse();
            return;
        }
        float radius = mix(0.14, 0.34, random01(cell, tierSalt + 4u)) * size;
        vec2 drift = windLean(root, matrices.wind, phase) * 0.03;
        vec2 driftBefore = windLean(root, matrices.windBefore, phase) * 0.03;
        vec3 centre = vec3(root.x + drift.x, grass.terrain.y + 0.012 + 0.006 * random01(cell, tierSalt + 5u), root.y + drift.y);
        vec3 padX = vec3(facing.x, 0.0, facing.y);
        vec3 up = vec3(0.0, 1.0, 0.0);
        vec3 world;
        if (part == 0) {
            // The rim curls up over its last quarter, more on some pads than others.
            vec2 p = aPos;
            float r = length(p);
            float lift = mix(0.002, 0.025, random01(cell, tierSalt + 11u)) * size;
            float s = clamp((r - 0.72) / 0.28, 0.0, 1.0);
            float ease = s * s * (3.0 - 2.0 * s);
            float slopeAcross = lift * 2.0 * ease * 6.0 * s * (1.0 - s) / 0.28 / max(radius, 1e-3);
            vec2 dir = r > 1e-4 ? p / r : vec2(0.0);
            world = centre + (padX * p.x + across * p.y) * radius + up * lift * ease * ease;
            vNormal = normalize(up - (padX * dir.x + across * dir.y) * slopeAcross);
            // Mostly deep green, some pads yellowing with age.
            vec3 green = mix(vec3(0.016, 0.032, 0.009), vec3(0.032, 0.048, 0.013), random01(cell, tierSalt + 9u));
            vColor = mix(green, vec3(0.060, 0.058, 0.016), step(0.85, random01(cell, tierSalt + 10u)) * 0.7);
            vPad = vec3(p, 0.0);
        }
        else {
            float bloom = mix(0.8, 1.2, random01(cell, tierSalt + 13u)) * size;
            vec3 heart = centre + (padX * 0.1 + across * 0.05) * radius + up * 0.006;
            if (part <= 2) {
                // Petals round the heart, the outer ring spread wide, the inner one cupped upright between them.
                bool inner = part == 2;
                float yaw2 = random01(cell, tierSalt + 14u) * 6.2831853 + (aTexCoords.y + (inner ? 0.5 : 0.0)) / 8.0 * 6.2831853;
                vec3 out3 = padX * cos(yaw2) + across * sin(yaw2);
                float rise = inner ? mix(0.9, 1.15, random01(cell, tierSalt + 15u)) : mix(0.15, 0.4, random01(cell, tierSalt + 16u));
                float petalLength = (inner ? 0.06 : 0.075) * bloom;
                vec3 axis = out3 * cos(rise) + up * sin(rise);
                vec3 side = normalize(cross(up, out3));
                float u = aPos.x;
                // Cupped: each petal bends up towards its tip.
                world = heart + out3 * 0.008 * bloom + axis * petalLength * u + up * petalLength * 0.25 * u * u
                    + side * aPos.y * petalLength * 0.2;
                vec3 normal = normalize(cross(side, axis + up * 0.5 * u));
                vNormal = normal.y < 0.0 ? -normal : normal;
                vColor = mix(vec3(0.34, 0.26, 0.24), vec3(0.62, 0.61, 0.58), smoothstep(0.0, 0.45, u));
                vPad = vec3(u, aPos.y, inner ? 2.0 : 1.0);
            }
            else {
                world = heart + up * 0.018 * bloom + (padX * aPos.x + across * aPos.y) * 0.02 * bloom;
                vNormal = up;
                vColor = vec3(0.46, 0.33, 0.03);
                vPad = vec3(aPos, 3.0);
            }
        }
        vec3 worldBefore = world - vec3(drift.x - driftBefore.x, 0.0, drift.y - driftBefore.y);
        vWorld = world;
        vClip = matrices.PVNoJitter * vec4(world, 1.0);
        vPrevClip = matrices.prevPVNoJitter * vec4(worldBefore, 1.0);
        gl_Position = matrices.PV * vec4(world, 1.0);
        return;
    }

    collapse();
}
