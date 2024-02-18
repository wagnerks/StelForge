#version 330 core
out vec4 FragColor;

in vec3 fragPos;
in vec3 normal;
uniform vec3 viewPos;

uniform vec4 color = vec4(1.f, 0.f, 0.f, 1.0);
void main()
{ 

    vec3 viewDir  = normalize(viewPos - fragPos);
    float illum = (dot(normal, viewDir) + 1.0) * 0.5;

    FragColor = vec4(color.xyz * illum, color.a);
}
