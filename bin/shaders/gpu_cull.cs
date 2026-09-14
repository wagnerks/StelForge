#version 450 core
#include "sfe.glsl"
// One invocation per batch (all instances of one mesh in one draw group). It walks its own instances
// in order and packs the visible ones at the start of its range, so no two invocations share a write.
layout(local_size_x = 16) in;

layout(SFE_SET(0, 1), std430) readonly buffer modelMatrices {
    mat4 model[];
};

struct GpuMesh {
    uint firstIndex;
    uint indexCount;
    uint firstVertex;
    uint vertexCount;
    vec3 boundsMin;
    uint skinned;
    vec3 boundsMax;
    float _pad;
};
layout(SFE_SET(0, 26), std430) readonly buffer Meshes {
    GpuMesh meshes[];
};

struct SceneInstance {
    uint entity;
    uint mesh;
    uint material;
    uint _pad;
};
layout(SFE_SET(0, 27), std430) readonly buffer Instances {
    SceneInstance instances[];
};

struct Batch {
    uint firstInstance;
    uint instanceCount;
    uint mesh;
    uint _pad;
};
layout(SFE_SET(0, 28), std430) readonly buffer Batches {
    Batch batches[];
};

struct VisibleInstance {
    uint entity;
    uint material;
};
layout(SFE_SET(0, 29), std430) writeonly buffer Visible {
    VisibleInstance visible[];
};

struct DrawCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};
layout(SFE_SET(0, 30), std430) writeonly buffer Commands {
    DrawCommand commands[];
};

// mode 0: shapes are 6 planes (xyz normal, w distance), inside when dot(n, p) + w >= -radius.
// mode 1: shapes are spheres (xyz centre, w radius), visible when the bounds touch any of them.
layout(SFE_SET(0, 31), std430) readonly buffer CullView {
    uint mode;
    uint shapeCount;
    uint batchCount;
    uint _pad;
    vec4 shapes[16];
} view;

// Animation can move vertices past the bind pose, so skinned bounds get this much of their largest
// half-extent added on every axis.
const float kSkinnedInflate = 0.5;

bool visibleInView(vec3 center, vec3 extents) {
    if (view.mode == 0u) {
        for (uint i = 0u; i < view.shapeCount; ++i) {
            vec3 n = view.shapes[i].xyz;
            float r = dot(extents, abs(n));
            if (dot(n, center) + view.shapes[i].w < -r) {
                return false;
            }
        }
        return true;
    }
    for (uint i = 0u; i < view.shapeCount; ++i) {
        vec3 d = max(abs(center - view.shapes[i].xyz) - extents, vec3(0.0));
        if (dot(d, d) <= view.shapes[i].w * view.shapes[i].w) {
            return true;
        }
    }
    return false;
}

void main() {
    uint b = gl_GlobalInvocationID.x;
    if (b >= view.batchCount) {
        return;
    }
    Batch batch = batches[b];
    GpuMesh mesh = meshes[batch.mesh];
    vec3 localCenter = (mesh.boundsMin + mesh.boundsMax) * 0.5;
    vec3 localExtents = (mesh.boundsMax - mesh.boundsMin) * 0.5;

    uint count = 0u;
    for (uint i = 0u; i < batch.instanceCount; ++i) {
        SceneInstance instance = instances[batch.firstInstance + i];
        mat4 m = model[instance.entity];
        vec3 center = (m * vec4(localCenter, 1.0)).xyz;
        vec3 extents = abs(m[0].xyz) * localExtents.x + abs(m[1].xyz) * localExtents.y + abs(m[2].xyz) * localExtents.z;
        if (mesh.skinned != 0u) {
            extents += vec3(max(extents.x, max(extents.y, extents.z)) * kSkinnedInflate);
        }
        if (visibleInView(center, extents)) {
            visible[batch.firstInstance + count] = VisibleInstance(instance.entity, instance.material);
            ++count;
        }
    }

    commands[b] = DrawCommand(mesh.indexCount, count, mesh.firstIndex, int(mesh.firstVertex), batch.firstInstance);
}
