#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangents;
layout (location = 4) in vec3 aBiTangents;


out highp vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;
out vec3 ViewPos;
layout(std430, binding = 1) buffer modelMatrices
{
    mat4 model[];
};

out mat3 TBN;

uniform mat3 normalMatrix;

layout(std140, binding = 5) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;


void main()
{
    
    vec4 worldPos = model[gl_InstanceID] * vec4(aPos, 1.0);
    ViewPos = vec3(matrices.view * worldPos);

    FragPos = worldPos.xyz; 
    TexCoords = aTexCoords;

    Normal = normalize(transpose(inverse(mat3(model[gl_InstanceID]))) * aNormal);
    TBN = mat3 (aTangents, aBiTangents, Normal);

    gl_Position = matrices.PV * worldPos;
}
