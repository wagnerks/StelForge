// Material parameters, one entry per distinct material. Mirrors SFE::Render::GpuMaterial; the pass
// rules (dither cutoff, caster opacity, glass defaults) are resolved on the CPU when an entry is made.
struct SfeMaterial {
    float matRoughness;
    float matMetallic;
    float mapMask;
    float alphaCutoff;     // g-buffer: > 1 dithers
    float uvSel;
    float specFactor;
    float clearcoat;
    float clearcoatRough;
    vec3 tint;
    float opacity;         // glass density
    float ior;             // glass
    float casterOpacity;   // shadow casters
    float shadowCutoff;    // shadow casters: < 0 takes the mask from r/g
    float _pad;
};

layout(SFE_SET(0, 7), std430) readonly buffer SfeMaterials {
    SfeMaterial sfeMaterials[];
};
