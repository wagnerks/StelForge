#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;


struct Light {
    vec3 Position;
    vec3 Color;
    
    float Linear;
    float Quadratic;
    float Radius;
};
const int NR_LIGHTS = 10;
uniform Light lights[NR_LIGHTS];
uniform vec3 viewPos;

struct DirectionLight {
    mat4 PV; //proj * view from light perspective matrix
    sampler2D shadowsMap;
};

const int MAX_SHADOWS_SIZE = 5;
uniform DirectionLight DirLights[MAX_SHADOWS_SIZE];
uniform int shadowsCount;

void main()
{             
    // retrieve data from gbuffer
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;

    float shadowDepth = 1.f;
    for (int i = 0; i < shadowsCount; i++){
        vec4 shadowCoord = DirLights[i].PV * vec4(FragPos,1.0);
        vec3 projCoords = shadowCoord.xyz / shadowCoord.w * 0.5 + 0.5;

        shadowDepth *= texture(DirLights[i].shadowsMap, projCoords.xy).r;
    }
            
    // then calculate lighting as usual
    vec3 lighting  = Diffuse * 0.3; // hard-coded ambient component
    vec3 viewDir  = normalize(viewPos - FragPos);
    for(int i = 0; i < NR_LIGHTS; ++i)
    {
        // calculate distance between light source and current fragment
        float distance = length(lights[i].Position - FragPos);
        if(distance < lights[i].Radius)
        {
            // diffuse
            vec3 lightDir = normalize(lights[i].Position - FragPos);
            vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * lights[i].Color;
            // specular
            vec3 halfwayDir = normalize(lightDir + viewDir);  
            float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
            vec3 specular = lights[i].Color * spec * Specular;
            // attenuation
            float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);
            diffuse *= attenuation;
            specular *= attenuation;
            lighting += diffuse + specular;
        }
    }


    lighting *= shadowDepth;
    
    FragColor = vec4(lighting, 1.0);
}