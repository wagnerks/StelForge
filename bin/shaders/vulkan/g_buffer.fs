#version 450

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec2 TexCoords;
layout(location = 2) in mat3 TBN;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;

layout(set = 1, binding = 1) uniform sampler2D texture_diffuse1;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D texture_specular1;
layout(set = 1, binding = 4) uniform sampler2D texture_roughness;
layout(set = 1, binding = 5) uniform sampler2D texture_metallic;

void main() {
    gPosition.rgb = FragPos;
    gNormal.a = gl_FragCoord.z / gl_FragCoord.w;

    vec3 normal = texture(normalMap, TexCoords).rgb;
    gNormal.xyz = normalize(TBN * normalize(normal * 2.0 - 1.0));

    vec3 albedoSrgb = texture(texture_diffuse1, TexCoords).rgb;
    gAlbedoSpec.rgb = pow(albedoSrgb, vec3(2.2));

    float spec = texture(texture_specular1, TexCoords).r;
    float roughMap = texture(texture_roughness, TexCoords).g;
    if (roughMap < 0.001) {
        roughMap = texture(texture_roughness, TexCoords).r;
    }
    float roughness = 0.5;
    if (roughMap > 0.001) {
        roughness = roughMap;
    } else if (spec > 0.001) {
        roughness = 1.0 - spec;
    }
    float metallic = texture(texture_metallic, TexCoords).b;
    if (metallic < 0.001) {
        metallic = texture(texture_metallic, TexCoords).r;
    }

    gAlbedoSpec.a = clamp(roughness, 0.04, 1.0);
    gPosition.a = clamp(metallic, 0.0, 1.0);
}
