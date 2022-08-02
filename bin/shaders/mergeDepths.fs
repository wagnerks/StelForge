#version 330 core
layout(location = 0) out vec3 image; //need if we draw it into framebuffer texture
layout(location = 1) out vec3 image1; //need if we draw it into framebuffer texture
layout(location = 2) out vec3 image2; //need if we draw it into framebuffer texture
layout(location = 3) out vec3 image3; //need if we draw it into framebuffer texture
layout(location = 4) out vec3 image4; //need if we draw it into framebuffer texture

//out vec4 FragColor;  //need if we draw it on screen

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
        float tex = texture(lights[i].shadowsMap, projCoords.xy).r;
        if (i == 0){
            image1 = tex * color;
        }
        else if (i == 1){
            image2 = tex * color;
        }
        else if (i == 2){
            image3 = tex * color;
        }
        else if (i == 3){
            image4 = tex * color;
        }
        
        shadowDepth = min(shadowDepth,tex);
    }
        
    vec3 lighting = shadowDepth * color; 
    
    image = lighting;
    //FragColor = vec4(lighting, 1.0);
}