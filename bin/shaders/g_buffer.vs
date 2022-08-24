#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;


out highp vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;
out vec3 ViewPos;
layout(std430, binding = 1) buffer modelMatrices
{
    mat4 model[];
};

uniform mat3 normalMatrix;

uniform mat4 PV;
uniform mat4 V;

void main()
{
    
    vec4 worldPos = model[gl_InstanceID] * vec4(aPos, 1.0);
    ViewPos = vec3(V * worldPos);

    FragPos = worldPos.xyz; 
    TexCoords = aTexCoords;

    Normal = normalize(transpose(inverse(mat3(model[gl_InstanceID]))) * aNormal);

    gl_Position = PV * worldPos;
}