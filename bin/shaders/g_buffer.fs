#version 330 core
layout (location = 0) out highp vec4 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec2 TexCoords;
in highp vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform sampler2D normalMap;

uniform mat4 PV;
uniform mat4 P;
uniform mat4 V;


void main()
{    
    // store the fragment position vector in the first gbuffer texture
    gPosition.rgb = FragPos;
    gPosition.a = gl_FragCoord.a;
    // also store the per-fragment normals into the gbuffer
    gNormal = Normal;

    //  // obtain normal from normal map in range [0,1]
    // vec3 normal = texture(normalMap, TexCoords).rgb;
    // // transform normal vector to range [-1,1]
    // gNormal = Normal * normalize(normal * 2.0 - 1.0);   

    // and the diffuse per-fragment color
    gAlbedoSpec.rgb = texture(texture_diffuse1, TexCoords).rgb;
    // store specular intensity in gAlbedoSpec's alpha component
    gAlbedoSpec.a = texture(texture_specular1, TexCoords).r;
}