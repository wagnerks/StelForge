#version 450 core
#include "sfe.glsl"
#include "sfe_material.glsl"
layout(location = 1) in vec2 TexCoords;
layout(location = 2) in vec2 TexCoords1;
layout(location = 3) flat in uint vMaterial;

layout(SFE_SET(0, 4)) uniform sampler2D texture_diffuse1;

float ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

void main() {
    SfeMaterial material = sfeMaterials[vMaterial];
    vec4 t = textureLod(texture_diffuse1, TexCoords, 0.0);
    float cutoff = abs(material.shadowCutoff);
    float a = material.shadowCutoff < 0.0 ? max(t.r, t.g) : t.a;
    a = min(a, textureLod(texture_diffuse1, TexCoords1, 0.0).a);
    if (cutoff > 0.0 && a < cutoff) {
        discard;
    }
    float opacity = clamp(material.casterOpacity, 0.0, 1.0);
    if (opacity < 0.999 && ign(gl_FragCoord.xy) > opacity) {
        discard;
    }
}
