#version 450

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec2 TexCoords;
layout(location = 2) in vec3 Normal;
layout(location = 3) in vec3 ViewPos;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;

layout(set = 0, binding = 5, std140) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

layout(push_constant) uniform PushConstants {
    vec4 cameraPos;
    float farPlane;
    float nearPlane;
} pc;

vec4 grid(vec3 fragPos3D, float scale) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 gridLines = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(gridLines.x, gridLines.y);
    float minimumz = min(derivative.y, 1.0);
    float minimumx = min(derivative.x, 1.0);
    vec4 color = vec4(0.2, 0.2, 0.2, 2.5 - min(line, 1.0));

    if (fragPos3D.x > -0.5 * minimumx && fragPos3D.x < 0.5 * minimumx) {
        color.z = 1.0;
        color.a = 1.0;
    }
    if (fragPos3D.z > -0.5 * minimumz && fragPos3D.z < 0.5 * minimumz) {
        color.x = 1.0;
        color.a = 1.0;
    }

    return color;
}

float computeLinearDepth(vec3 pos) {
    vec4 clipSpacePos = matrices.PV * vec4(pos.xyz, 1.0);
    float clipSpaceDepth = (clipSpacePos.z / clipSpacePos.w) * 2.0 - 1.0;
    return ((2.0 * pc.nearPlane * pc.farPlane) / (pc.farPlane + pc.nearPlane - clipSpaceDepth * (pc.farPlane - pc.nearPlane))) / pc.farPlane;
}

void main() {
    gPosition.rgb = FragPos;
    gPosition.a = 0.0;
    gNormal.xyz = vec3(0.0, 1.0, 0.0);
    gNormal.a = gl_FragCoord.z / gl_FragCoord.w;
    gAlbedoSpec.a = 0.28;

    float distanceKoef = min(abs(pc.cameraPos.y), pc.farPlane) / pc.farPlane;

    vec4 color1 = grid(FragPos.xyz, 1.0);
    color1.a *= 1.0 - distanceKoef * 40.0;
    color1.a *= 0.8;

    vec4 color2 = grid(FragPos.xyz, 0.1);
    color2.a *= 1.0 - distanceKoef * 10.0;

    vec4 color3 = grid(FragPos.xyz, 0.01);

    gAlbedoSpec.rgb = color1.rgb;
    gAlbedoSpec.rgb = mix(color2.rgb, gAlbedoSpec.rgb, 1.0);
    gAlbedoSpec.rgb = mix(color3.rgb, gAlbedoSpec.rgb, 1.0);
    gAlbedoSpec.rgb *= vec3(max(color1.a, max(color2.a, color3.a)));
    gAlbedoSpec.rgb *= 3.0;

    float linearDepth = computeLinearDepth(FragPos.xyz);
    float fading = max(0.0, 0.8 - linearDepth);
    gAlbedoSpec.rgb *= vec3(fading);
}
