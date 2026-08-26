#version 430

layout (binding = 0) uniform sampler2D ssaoInput;

in vec2 TexCoords;
layout(std140, binding = 1) uniform SSAOParams {
    float sigmaS;
    float sigmaL;

    float facS;
    float facL;
} params;

out float FragColor;

float lum(in vec4 color) {
    return length(color.xyz);
}

void main()
{
  float sumW = 0.;
  float sumC = 0.;
  int halfSize = int(params.sigmaS) * 2;
  vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));

  float l = lum(texture(ssaoInput,TexCoords));

  for (int i = -halfSize; i <= halfSize; i ++){
    for (int j = -halfSize; j <= halfSize; j ++){
      vec2 pos = vec2(float(i), float(j));
      vec2 offset = pos * texelSize;
      vec4 offsetColor = texture(ssaoInput, TexCoords + offset);

      float distS = length(pos);
      float distL = lum(offsetColor)-l;

      float wS = exp(params.facS*float(distS*distS));
      float wL = exp(params.facL*float(distL*distL));
      float w = wS*wL;

      sumW += w;
      sumC += offsetColor.r * w;
    }
  }
  
//     vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
//     float result = 0.0;
//     for (int x = -2; x < 2; ++x) 
//     {
//         for (int y = -2; y < 2; ++y) 
//         {
//             vec2 offset = vec2(float(x), float(y)) * texelSize;
//             result += texture(ssaoInput, TexCoords + offset).r;
//         }
//     }

  FragColor = sumC/sumW;
}