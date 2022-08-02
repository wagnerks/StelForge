#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;


out vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;
layout(std430, binding = 1) buffer modelMatrices
{
    mat4 model[];
};

uniform mat3 normalMatrix;

uniform mat4 PV;

void main()
{
    vec4 worldPos = model[gl_InstanceID] * vec4(aPos, 1.0);
    FragPos = worldPos.xyz; 
    TexCoords = aTexCoords;

    Normal = normalize(transpose(inverse(mat3(model[gl_InstanceID]))) * aNormal);

    gl_Position = PV * worldPos;
}