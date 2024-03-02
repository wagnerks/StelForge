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

uniform float far;
uniform vec3 cameraPos;

mat4 scaleMatrix = mat4(
    far*2.0, 0.0,   0.0,   0.0,
    0.0,   far*2.0, 0.0,   0.0,
    0.0,   0.0,   far*2.0, 0.0,
    0.0,   0.0,   0.0,   1.0
);

mat4 transformMatrix = mat4(
    1.0, 0.0,   0.0,   0.0,
    0.0,   1.0, 0.0,   0.0,
    0.0,   0.0,   1.0, 0.0,
    cameraPos.x,   0.0,   cameraPos.z,   1.0
);

void main() {
    vec4 worldPos = transformMatrix * scaleMatrix * vec4(aPos, 1.0);

    ViewPos = vec3(matrices.view * worldPos);

    FragPos = worldPos.xyz; 
    TexCoords = aTexCoords;
    Normal = aNormal;

    gl_Position = matrices.PV * worldPos;
}
