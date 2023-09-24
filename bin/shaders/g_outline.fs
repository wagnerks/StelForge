#version 330 core
layout (location = 0) out vec4 gOutlines;

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D gDepth;//this is a normal texture where i store depth in a
uniform sampler2D gOutlinesP;
uniform sampler2D gLightsP;

void main()
{    
    vec2 texSize  = textureSize(gOutlinesP, 0).xy;
    // Sobel Operator
    float xStep = 1.0;
    xStep = xStep / texSize.x;

    float yStep = 1.0;
    yStep = yStep / texSize.y;

    float gx = texture(gDepth, TexCoord - vec2(xStep,    0.0)).a * 1.0 +
               texture(gDepth, TexCoord - vec2(xStep,  yStep)).a * 2.0 +
               texture(gDepth, TexCoord - vec2(xStep, -yStep)).a * 2.0 -
               texture(gDepth, TexCoord + vec2(xStep,    0.0)).a * 1.0 -
               texture(gDepth, TexCoord + vec2(xStep,  yStep)).a * 2.0 -
               texture(gDepth, TexCoord + vec2(xStep, -yStep)).a * 2.0;
    
    float gy = texture(gDepth, TexCoord - vec2(0.0,    yStep)).a * 1.0 +
               texture(gDepth, TexCoord - vec2(xStep,  yStep)).a * 2.0 +
               texture(gDepth, TexCoord + vec2(0.0,    yStep)).a * 1.0 -
               texture(gDepth, TexCoord - vec2(0.0,   -yStep)).a * 1.0 -
               texture(gDepth, TexCoord + vec2(xStep, -yStep)).a * 2.0 -
               texture(gDepth, TexCoord + vec2(0.0,   -yStep)).a * 1.0;
    
    // Calculate edge strength
    float edge = sqrt(gx * gx + gy * gy);
    
    // You can apply a threshold to control the sensitivity of edge detection
    float threshold = 4.5; // Adjust as needed

    gOutlines = vec4(0.0);

    if (edge > threshold) {
        gOutlines.r = 1.0;
    }
   
    gOutlines.g = texture(gOutlinesP, TexCoord).g;

    int size = 4;

    float separation = 2.0;

    gOutlines.b = 0.0;

    float count = 0.0;

    for (int i = -size; i <= size; ++i) {
        for (int j = -size; j <= size; ++j) {
            vec2 pos = TexCoord + vec2(i, j)/texSize * separation;

            if (pos.x < 0.0 || pos.x > 1.0 || pos.y < 0.0 || pos.y > 1.0){
                continue;
            }
            gOutlines.b += texture(gOutlinesP, pos).g;
            count += 0.5;
        }
    }

    gOutlines.b /= count; 
    if (gOutlines.g > 0.0){
        gOutlines.b = 0.0;
    }

    size = 16;
    separation = 0.5;
    for (int i = -size; i <= size; ++i) {
        for (int j = -size; j <= size; ++j) {
            vec2 pos = TexCoord + vec2(i, j)/texSize * separation;

            if (pos.x < 0.0 || pos.x > 1.0 || pos.y < 0.0 || pos.y > 1.0){
                continue;
            }
            
            gOutlines.a += texture(gLightsP, pos).r;
            count += 0.5;
        }
    }

    gOutlines.a /= count;

}