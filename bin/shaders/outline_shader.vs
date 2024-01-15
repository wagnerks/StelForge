#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangents;
layout (location = 4) in vec3 aBiTangents;

uniform mat4 PV;
out vec3 texPos;

layout(std430, binding = 1) buffer modelMatrices
{
    mat4 model[];
};

void main()
{
    texPos = aPos;
    mat4 m = model[gl_InstanceID];
    gl_Position = PV * m  * vec4(aPos + aNormal* 5.0,1.0); 
}  
