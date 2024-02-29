#version 330 core
out vec4 FragColor;

in vec3 fragPos;
in vec3 normal;
in vec4 fragColor;
uniform vec3 viewPos;

void main()
{ 

    vec3 viewDir  = normalize(viewPos - fragPos);
    float illum = (dot(normal, viewDir) + 1.0) * 0.5;

    FragColor = vec4(fragColor.xyz * illum, fragColor.a);
}
