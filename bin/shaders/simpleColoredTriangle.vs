#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform vec3 viewPos;
uniform mat4 PVM;
out vec3 fragPos;
out vec3 normal;

void main()
{
    normal = aNormal;
    fragPos = aPos;

    vec3 viewDir  = normalize(viewPos - fragPos);

    gl_Position = PVM * vec4(aPos, 1.0); 
}  
