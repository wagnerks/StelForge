// One record per drawn instance, addressed by SFE_INSTANCE_ID. Include after sfe.glsl.
struct SfeInstance {
    uint entity;     // index into the transform buffer
    uint material;   // index into sfe_material.glsl's table
    uint boneOffset; // first of the entity's matrices in the bone buffer
    uint _pad;
};

layout(SFE_SET(0, 3), std430) readonly buffer SfeInstances {
    SfeInstance sfeInstances[];
};
