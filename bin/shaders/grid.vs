#version 450 core
#include "sfe.glsl"
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec2 TexCoords;
layout(location = 2) out vec3 Normal;
layout(location = 3) out vec3 ViewPos;
layout(location = 4) out vec4 vClip;
layout(location = 5) out vec4 vPrevClip;

layout(SFE_SET(0, 5), std140) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

SFE_PUSH_BLOCK PushConstants {
    vec4 cameraPos;
    float farPlane;
    float nearPlane;
} pc;

void main() {
    float s = max(pc.farPlane * 0.7, 200.0);
    mat4 scaleMatrix = mat4(
        s, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, s, 0.0,
        0.0, 0.0, 0.0, 1.0
    );

    mat4 transformMatrix = mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        pc.cameraPos.x, 0.0, pc.cameraPos.z, 1.0
    );

    vec4 worldPos = transformMatrix * scaleMatrix * vec4(aPos, 1.0);

    ViewPos = vec3(matrices.view * worldPos);
    FragPos = worldPos.xyz;
    TexCoords = aTexCoords;
    Normal = aNormal;

    // The grid does not move, so only the camera moves it on screen.
    vClip = matrices.PVNoJitter * worldPos;
    vPrevClip = matrices.prevPVNoJitter * worldPos;
    gl_Position = matrices.PV * worldPos;
}
