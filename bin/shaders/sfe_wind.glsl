#ifndef SFE_WIND_GLSL
#define SFE_WIND_GLSL
// How the wind moves a vertex of whatever sways in it. Include after sfe.glsl, and after sfe_material_table.glsl
// unless SFE_WIND_TREES_ONLY is defined.

// Gusts roll over the land in the direction the wind blows, so a field bows in waves, not all at once. 0.2..1 at
// `root`, a point on the ground; `wind` as below.
float sfeWindGust(vec2 root, vec4 wind) {
    float travel = dot(root, wind.xz);
    return 0.6 + 0.4 * sin(travel * 0.06 - wind.w * 1.1) * sin(travel * 0.021 - wind.w * 0.43 + 1.3);
}

// Each plant a little out of step with its neighbours.
float sfeWindPhase(vec2 root) {
    return float(sfeCellHash(ivec2(floor(root * 4.0))) >> 8u) / 16777216.0 * 6.2831853;
}

// A tree in the wind: a stiff trunk and a supple crown. `wind` is the shared block's: xz the unit direction it blows
// along, y its strength (0..1), w seconds. `root` is where the trunk stands. Impostors sway by this too, so a tree
// far away leans like the one that replaces it up close.
vec3 sfeWindSwayTree(vec3 worldPos, vec3 root, vec4 wind) {
    float strength = wind.y;
    if (strength <= 0.0) {
        return vec3(0.0);
    }
    vec2 along = wind.xz;
    vec2 across = vec2(-along.y, along.x);
    float seconds = wind.w;
    float height = max(worldPos.y - root.y, 0.0);
    float gust = sfeWindGust(root.xz, wind);
    float phase = sfeWindPhase(root.xz);

    // The bend grows with the square of the height, and past 12 metres, where a tall trunk stiffens, only with it.
    float bend = min(height * height * 0.0045, height * 0.054) * strength;
    vec2 push = along * bend * gust + across * bend * 0.3 * sin(seconds * 0.9 + phase);
    push += along * bend * 0.25 * sin(seconds * 1.7 + phase);
    // The leaves shiver on their own, faster and smaller.
    float leaves = min(height * 0.12, 1.0) * 0.03 * strength;
    vec3 flutter = vec3(sin(seconds * 4.3 + worldPos.x * 0.9 + phase),
                        sin(seconds * 5.1 + worldPos.z * 1.1) * 0.5,
                        cos(seconds * 3.7 + worldPos.y * 0.7 + phase)) * leaves;
    float lift = dot(push, push) / max(2.0 * height, 0.5);
    return vec3(push.x, -lift, push.y) + flutter;
}

#ifndef SFE_WIND_TREES_ONLY
// `root` is where the swaying thing stands (its entity's origin), `material` its entry in the material table.
vec3 sfeWindSway(vec3 worldPos, vec3 root, uint material, vec4 wind) {
    uint mask = uint(sfeMaterials[material].mapMask + 0.5);
    bool tree = (mask & 32u) != 0u;
    bool plant = (mask & 64u) != 0u;
    float strength = wind.y;
    if ((!tree && !plant) || strength <= 0.0) {
        return vec3(0.0);
    }
    if (tree) {
        return sfeWindSwayTree(worldPos, root, wind);
    }
    // Grass and bushes lean with the wind from the ground up and spring back quickly.
    vec2 along = wind.xz;
    vec2 across = vec2(-along.y, along.x);
    float seconds = wind.w;
    float height = max(worldPos.y - root.y, 0.0);
    float gust = sfeWindGust(root.xz, wind);
    float phase = sfeWindPhase(root.xz);

    float bend = (height * 0.18 + height * height * 0.08) * strength;
    vec2 push = along * bend * (gust + 0.35 * sin(seconds * 2.6 + phase)) + across * bend * 0.25 * sin(seconds * 3.1 + phase * 1.7);
    float lift = dot(push, push) / max(2.0 * height, 0.1);
    return vec3(push.x, -lift, push.y);
}
#endif

#endif
