#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec2 TexCoords;
layout(location = 2) out vec3 Normal;
layout(location = 3) out vec3 ViewPos;

layout(set = 0, binding = 5, std140) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

layout(push_constant) uniform PushConstants {
    vec4 cameraPos;
    float farPlane;
    float nearPlane;
} pc;

void main() {
    mat4 scaleMatrix = mat4(
        pc.farPlane * 2.0, 0.0, 0.0, 0.0,
        0.0, pc.farPlane * 2.0, 0.0, 0.0,
        0.0, 0.0, pc.farPlane * 2.0, 0.0,
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

    gl_Position = matrices.PV * worldPos;
}
