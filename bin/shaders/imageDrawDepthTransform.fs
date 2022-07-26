#version 330 core
out vec4 FragColor;

in VS_OUT {
    mat4 FragPosLightSpace;
} fs_in;

uniform sampler2D tex;
uniform sampler2D uPos;
in vec2 TexCoords;

void main()
{           
    vec3 pos = texture(uPos, TexCoords).rgb;
    
    vec3 color = vec3(1.f,1.f,1.f);
    vec4 shadowCoord = fs_in.FragPosLightSpace * vec4(pos,1.0);
    // perform perspective divide
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w * 0.5 + 0.5;

    float shadow = texture(tex, projCoords.xy).r;                
    vec3 lighting = ( shadow) * color;
    
    FragColor = vec4(lighting, 1.0);
}