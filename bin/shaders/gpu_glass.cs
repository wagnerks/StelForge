#version 450 core
#include "sfe.glsl"
// Glass blends over what is behind it, so it is drawn far to near, one command per instance. Three
// stages over the same buffers, chosen by the push constants:
//   0 key:  cull each instance against the view and write its distance along the view direction
//   1 sort: one compare-exchange step of a bitonic sort, far first, culled instances last
//   2 emit: write the visible records and one draw command per sorted position
layout(local_size_x = 64) in;

layout(SFE_SET(0, 1), std430) readonly buffer modelMatrices {
    mat4 model[];
};

#include "sfe_gpu_meshes.glsl"

struct SceneInstance {
    uint entity;
    uint mesh;
    uint material;
    uint boneOffset;
};
layout(SFE_SET(0, 27), std430) readonly buffer Instances {
    SceneInstance instances[];
};

struct VisibleInstance {
    uint entity;
    uint material;
    uint boneOffset;
    uint _pad;
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

// shapes[0..5]: frustum planes; shapes[6].xyz: camera position; shapes[7].xyz: view direction.
layout(SFE_SET(0, 31), std430) readonly buffer CullView {
    uint mode;
    uint shapeCount;
    uint batchCount;
    uint _pad;
    vec4 shapes[16];
} view;

struct SortKey {
    float depth;   // larger is farther; culled instances hold kCulled
    uint instance;
};
layout(SFE_SET(0, 32), std430) buffer Keys {
    SortKey keys[];
};

SFE_PUSH_BLOCK GlassStep {
    uint stage;
    uint count;       // real instances
    uint paddedCount; // power of two, the sort's range
    uint j;
    uint k;
    uint base;        // first instance slot of the glass batch
} step;

const float kCulled = -3.0e38;

bool visibleInView(vec3 center, vec3 extents) {
    for (int i = 0; i < 6; ++i) {
        vec3 n = view.shapes[i].xyz;
        if (dot(n, center) + view.shapes[i].w < -dot(extents, abs(n))) {
            return false;
        }
    }
    return true;
}

// Far first; ties by instance index, so the order does not depend on the sort network.
bool before(SortKey a, SortKey b) {
    return a.depth != b.depth ? a.depth > b.depth : a.instance < b.instance;
}

void main() {
    uint i = gl_GlobalInvocationID.x;

    if (step.stage == 0u) {
        if (i >= step.paddedCount) {
            return;
        }
        if (i >= step.count) {
            keys[i] = SortKey(kCulled, 0xFFFFFFFFu);
            return;
        }
        SceneInstance instance = instances[step.base + i];
        GpuMesh mesh = meshes[instance.mesh];
        mat4 m = model[instance.entity];
        vec3 localCenter = (mesh.boundsMin + mesh.boundsMax) * 0.5;
        vec3 localExtents = (mesh.boundsMax - mesh.boundsMin) * 0.5;
        vec3 center = (m * vec4(localCenter, 1.0)).xyz;
        vec3 extents = abs(m[0].xyz) * localExtents.x + abs(m[1].xyz) * localExtents.y + abs(m[2].xyz) * localExtents.z;
        if (mesh.skinned != 0u) {
            extents = sfeSkinnedInflated(extents);
        }
        // The bounds' centre, not the entity origin: submeshes of one entity, and imported glass whose pivot
        // sits at the origin, would all share one depth and fall back to slot order.
        float depth = dot(center - view.shapes[6].xyz, view.shapes[7].xyz);
        keys[i] = SortKey(visibleInView(center, extents) ? depth : kCulled, step.base + i);
        return;
    }

    if (step.stage == 1u) {
        uint l = i ^ step.j;
        if (i >= step.paddedCount || l <= i) {
            return;
        }
        SortKey a = keys[i];
        SortKey b = keys[l];
        bool ascending = (i & step.k) == 0u;
        if (ascending ? before(b, a) : before(a, b)) {
            keys[i] = b;
            keys[l] = a;
        }
        return;
    }

    if (i >= step.count) {
        return;
    }
    SortKey key = keys[i];
    if (key.depth == kCulled) {
        commands[i] = DrawCommand(0u, 0u, 0u, 0, i);
        return;
    }
    SceneInstance instance = instances[key.instance];
    GpuMesh mesh = meshes[instance.mesh];
    visible[i] = VisibleInstance(instance.entity, instance.material, instance.boneOffset, 0u);
    commands[i] = DrawCommand(mesh.indexCount, 1u, mesh.firstIndex, int(mesh.firstVertex), i);
}
