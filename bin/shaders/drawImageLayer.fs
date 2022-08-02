#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
uniform sampler2DArray tex;

void main()
{             
    FragColor = vec4(texture(tex, vec3(TexCoords,0)).rgb, 1.f);
    //FragColor = vec4(1.f,1.f,1.f, 1.f);
}