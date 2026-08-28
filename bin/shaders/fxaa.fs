#version 430 core

out vec4 FragColor;
in vec2 TexCoords;

layout(binding = 0) uniform sampler2D inputTexture;

float luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(inputTexture, 0));
    vec3 rgbM = texture(inputTexture, TexCoords).rgb;

    float lM = luma(rgbM);
    float lN = luma(texture(inputTexture, TexCoords + vec2(0.0, -texel.y)).rgb);
    float lS = luma(texture(inputTexture, TexCoords + vec2(0.0,  texel.y)).rgb);
    float lW = luma(texture(inputTexture, TexCoords + vec2(-texel.x, 0.0)).rgb);
    float lE = luma(texture(inputTexture, TexCoords + vec2( texel.x, 0.0)).rgb);

    float lMin = min(lM, min(min(lN, lS), min(lW, lE)));
    float lMax = max(lM, max(max(lN, lS), max(lW, lE)));
    float range = lMax - lMin;
    if (range < max(0.0312, lMax * 0.125)) {
        FragColor = vec4(rgbM, 1.0);
        return;
    }

    float lNW = luma(texture(inputTexture, TexCoords + vec2(-texel.x, -texel.y)).rgb);
    float lNE = luma(texture(inputTexture, TexCoords + vec2( texel.x, -texel.y)).rgb);
    float lSW = luma(texture(inputTexture, TexCoords + vec2(-texel.x,  texel.y)).rgb);
    float lSE = luma(texture(inputTexture, TexCoords + vec2( texel.x,  texel.y)).rgb);

    vec2 dir;
    dir.x = -((lN + lS) - (lW + lE));
    dir.y =  ((lE + lW) - (lN + lS));
    float dirReduce = max((lN + lS + lW + lE) * 0.03125, 0.0078125);
    float rcpDir = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDir, vec2(-8.0), vec2(8.0)) * texel;

    vec3 rgbA = 0.5 * (
        texture(inputTexture, TexCoords + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(inputTexture, TexCoords + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(inputTexture, TexCoords + dir * -0.5).rgb +
        texture(inputTexture, TexCoords + dir *  0.5).rgb);

    float lB = luma(rgbB);
    FragColor = vec4((lB < lMin || lB > lMax) ? rgbA : rgbB, 1.0);
}
