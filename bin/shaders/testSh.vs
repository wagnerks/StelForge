#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out highp vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;
out vec3 ViewPos;

layout(std140, binding = 5) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

layout(std140, binding = 15) uniform PushConstants {
    vec4 cameraPos;
    float far;
    float near;
} pc;


mat4 scaleMatrix = mat4(
    200.0, 0.0,   0.0,   0.0,
    0.0,   1.0,   0.0,   0.0,
    0.0,   0.0,   200.0, 0.0,
    0.0,   0.0,   0.0,   1.0
);

mat4 transformMatrix = mat4(
    1.0, 0.0,   0.0,   0.0,
    0.0,   1.0, 0.0,   0.0,
    0.0,   0.0,   1.0, 0.0,
    pc.cameraPos.x,   0.0,   pc.cameraPos.z,   1.0
);

void main() {
    vec4 worldPos = transformMatrix * scaleMatrix * vec4(aPos, 1.0);

    ViewPos = vec3(matrices.view * worldPos);

    FragPos = worldPos.xyz; 
    TexCoords = aTexCoords;
    Normal = aNormal;

    gl_Position = matrices.PV * worldPos;
}
