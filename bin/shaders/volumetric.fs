#version 430 core

out vec4 FragColor;
in vec2 TexCoords;

layout(binding = 0) uniform sampler2D worldPos;
layout(binding = 8) uniform sampler2DArrayShadow shadowMap;

layout (std140, binding = 11) uniform LightSpaceMatrices {
    mat4 lightSpaceMatrices[6];
};

layout(std140, binding = 5) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

layout(std140, binding = 15) uniform PushConstants {
    vec3 viewPos;
    float density;
    vec3 lightDir;
    float anisotropy;
    vec3 lightColor;
    float intensity;
    vec2 resolution;
    int cascadeCount;
    float maxDistance;
} params;

const int STEPS = 16;
const float PI = 3.14159265;

float ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

float henyeyGreenstein(float cosTheta, float g) {
    float g2 = g * g;
    float denom = pow(max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4), 1.5);
    return (1.0 - g2) / (4.0 * PI * denom);
}

float sunVisibility(vec3 worldP) {
    int cc = params.cascadeCount < 1 ? 4 : params.cascadeCount;
    int layer = 0;
    vec3 proj = vec3(0.0);
    for (; layer < cc; ++layer) {
        vec4 ls = lightSpaceMatrices[layer] * vec4(worldP, 1.0);
        proj = ls.xyz / max(ls.w, 1e-4);
        proj = proj * 0.5 + 0.5;
        if (proj.x > 0.0 && proj.x < 1.0 && proj.y > 0.0 && proj.y < 1.0 && proj.z > 0.0 && proj.z < 1.0) {
            break;
        }
    }
    if (layer >= cc) {
        return 1.0;
    }
    return texture(shadowMap, vec4(proj.xy, float(layer), proj.z - 0.0015));
}

vec3 cameraRay(vec2 uv) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 viewP = inverse(matrices.projection) * ndc;
    viewP /= max(viewP.w, 1e-4);
    return normalize(mat3(inverse(matrices.view)) * viewP.xyz);
}

void main() {
    vec3 posSample = texture(worldPos, TexCoords).rgb;
    vec3 origin = params.viewPos;
    vec3 rayDir;
    float rayLen;
    if (dot(posSample, posSample) > 1e-4) {
        vec3 delta = posSample - origin;
        rayLen = min(length(delta), params.maxDistance);
        rayDir = delta / max(length(delta), 1e-4);
    } else {
        rayDir = cameraRay(TexCoords);
        rayLen = params.maxDistance;
    }

    float stepLen = rayLen / float(STEPS);
    float t = ign(gl_FragCoord.xy) * stepLen;
    vec3 L = normalize(-params.lightDir);
    float phase = henyeyGreenstein(dot(rayDir, L), clamp(params.anisotropy, -0.9, 0.9));
    vec3 inScatter = vec3(0.0);
    float trans = 1.0;
    float dens = max(params.density, 0.0);

    for (int i = 0; i < STEPS; ++i) {
        vec3 p = origin + rayDir * min(t, rayLen);
        float vis = sunVisibility(p);
        inScatter += params.lightColor * params.intensity * dens * phase * vis * stepLen * trans;
        trans *= exp(-dens * stepLen);
        t += stepLen;
    }

    FragColor = vec4(inScatter, trans);
}
