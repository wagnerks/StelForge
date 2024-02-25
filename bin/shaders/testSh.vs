#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out highp vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;
out vec3 ViewPos;

layout(std140, binding = 5) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

layout(std430, binding = 1) buffer modelMatrices {
    mat4 model[];
};

uniform float far;

mat4 scaleMatrix = mat4(
    far*2.0, 0.0,   0.0,   0.0,
    0.0,   far*2.0, 0.0,   0.0,
    0.0,   0.0,   far*2.0, 0.0,
    0.0,   0.0,   0.0,   1.0
);

void main() {
    vec4 worldPos = model[gl_InstanceID] * scaleMatrix * vec4(aPos.xyz, 1.0);

    ViewPos = vec3(matrices.view * worldPos);

    FragPos = worldPos.xyz; 
    TexCoords = aTexCoords;
    Normal = aNormal;

    gl_Position = matrices.PV * worldPos;
}
