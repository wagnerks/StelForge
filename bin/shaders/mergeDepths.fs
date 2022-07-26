#version 330 core
out vec4 FragColor;

uniform sampler2D uPos;
in vec2 TexCoords;
const vec3 color = vec3(1.f,1.f,1.f);

struct DirectionLight {
    mat4 PV; //proj * view from light perspective matrix
    sampler2D shadowsMap;
};

const int MAX_SHADOWS_SIZE = 5;
uniform DirectionLight lights[MAX_SHADOWS_SIZE];
uniform int shadowsCount;

void main()
{        
    vec3 pos = texture(uPos, TexCoords).rgb;    
    float shadowDepth = 1.f;
    for (int i = 0; i < shadowsCount; i++){
        vec4 shadowCoord = lights[i].PV * vec4(pos,1.0);
        vec3 projCoords = shadowCoord.xyz / shadowCoord.w * 0.5 + 0.5;

        shadowDepth *= texture(lights[i].shadowsMap, projCoords.xy).r;
    }
        
    vec3 lighting = shadowDepth * color; 

    FragColor = vec4(lighting, 1.0);
}