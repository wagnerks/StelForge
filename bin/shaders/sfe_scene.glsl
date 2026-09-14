// One record per drawn instance, addressed by SFE_INSTANCE_ID. Include after sfe.glsl.
struct SfeInstance {
    uint entity;   // index into the transform and bone buffers
    uint material; // index into sfe_material.glsl's table
};

layout(SFE_SET(0, 3), std430) readonly buffer SfeInstances {
    SfeInstance sfeInstances[];
};
