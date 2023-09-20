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
    vec3 pos = aPos;
    //pos.x = aPos.x+1.0;
    //pos.y = aPos.y + 1.0;
    mat4 m = model[gl_InstanceID];
    float scale = 1.1;

    mat4 scaleMatrix;
    scaleMatrix[0].xyzw = vec4(scale, 0.0,   0.0,   0.0);
    scaleMatrix[1].xyzw = vec4(0.0,   scale, 0.0,   0.0);
    scaleMatrix[2].xyzw = vec4(0.0,   0.0,   scale, 0.0);
    scaleMatrix[3].xyzw = vec4(0.0,   0.0,   0.0,   1.0);
   
    gl_Position = PV * m  * vec4(pos + aNormal* 5.0,1.0); 
}  
