// #version 330 core
// out float FragColor;

// in vec2 TexCoords;

// uniform sampler2D ssaoInput;

// void main() 
// {
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
//     FragColor = result / (4.0 * 4.0);
// }  


// #version 150

// #define MAX_SIZE        4
// #define MAX_KERNEL_SIZE ((MAX_SIZE * 2 + 1) * (MAX_SIZE * 2 + 1))
// #define MAX_BINS_SIZE   10

// uniform sampler2D ssaoInput;
// in vec2 TexCoords;

// uniform vec2 parameters = vec2(1.0,1.0);

// out float fragColor;

// void main() {
//   vec2 texSize  = textureSize(ssaoInput, 0).xy;
//   vec2 texCoord = TexCoords.xy / texSize;

//   int size = int(parameters.x);
//   if (size <= 0) { 
//     fragColor = texture(ssaoInput, texCoord).r; 
//     return; 
//   }

//   if (size > MAX_SIZE) { size = MAX_SIZE; }
//   int kernelSize = int(pow(size * 2 + 1, 2));

//   int binsSize = int(parameters.y);
//       binsSize = clamp(binsSize, 1, MAX_BINS_SIZE);

//   int i        = 0;
//   int j        = 0;
//   int count    = 0;
//   int binIndex = 0;

//   vec4  colors[MAX_KERNEL_SIZE];
//   float bins[MAX_BINS_SIZE];
//   int   binIndexes[colors.length()];

//   float total = 0;
//   float limit = floor(float(kernelSize) / 2) + 1;

//   float value       = 0;
//   vec3  valueRatios = vec3(0.3, 0.59, 0.11);

//   for (i = -size; i <= size; ++i) {
//     for (j = -size; j <= size; ++j) {
//       colors[count] =
//         texture
//           ( ssaoInput
//           ,   ( TexCoords.xy
//               + vec2(i, j)
//               )
//             / texSize
//           );
//       count += 1;
//     }
//   }

//   for (i = 0; i < binsSize; ++i) {
//     bins[i] = 0;
//   }

//   for (i = 0; i < kernelSize; ++i) {
//     value           = dot(colors[i].rgb, valueRatios);
//     binIndex        = int(floor(value * binsSize));
//     binIndex        = clamp(binIndex, 0, binsSize - 1);
//     bins[binIndex] += 1;
//     binIndexes[i]   = binIndex;
//   }

//   binIndex = 0;

//   for (i = 0; i < binsSize; ++i) {
//     total += bins[i];
//     if (total >= limit) {
//       binIndex = i;
//       break;
//     }
//   }

//   fragColor = colors[0].r;

//   for (i = 0; i < kernelSize; ++i) {
//     if (binIndexes[i] == binIndex) {
//       fragColor = colors[i].r;
//       break;
//     }
//   }
// }




// #version 150

// #define MAX_SIZE        5
// #define MAX_KERNEL_SIZE ((MAX_SIZE * 2 + 1) * (MAX_SIZE * 2 + 1))

// uniform sampler2D ssaoInput;
// in vec2 TexCoords;

// uniform vec2 parameters;

// out float fragColor;

// vec2 texSize  = textureSize(ssaoInput, 0).xy;
// vec2 texCoord = TexCoords.xy / texSize;

// int i     = 0;
// int j     = 0;
// int count = 0;

// vec3  valueRatios = vec3(0.3, 0.59, 0.11);

// float values[MAX_KERNEL_SIZE];

// vec4  color       = vec4(0.0);
// vec4  meanTemp    = vec4(0.0);
// vec4  mean        = vec4(0.0);
// float valueMean   =  0.0;
// float variance    =  0.0;
// float minVariance = -1.0;

// void findMean(int i0, int i1, int j0, int j1) {
//   meanTemp = vec4(0);
//   count    = 0;

//   for (i = i0; i <= i1; ++i) {
//     for (j = j0; j <= j1; ++j) {
//       color  =
//         texture
//           ( ssaoInput
//           ,   (TexCoords.xy + vec2(i, j))
//             / texSize
//           );

//       meanTemp += color;

//       values[count] = dot(color.rgb, valueRatios);

//       count += 1;
//     }
//   }

//   meanTemp.rgb /= count;
//   valueMean     = dot(meanTemp.rgb, valueRatios);

//   for (i = 0; i < count; ++i) {
//     variance += pow(values[i] - valueMean, 2);
//   }

//   variance /= count;

//   if (variance < minVariance || minVariance <= -1) {
//     mean = meanTemp;
//     minVariance = variance;
//   }
// }

// void main() {
//   fragColor = texture(ssaoInput, texCoord).r;

//   int size = int(parameters.x);
//   if (size <= 0) { return; }

//   // Lower Left

//   findMean(-size, 0, -size, 0);

//   // Upper Right

//   findMean(0, size, 0, size);

//   // Upper Left

//   findMean(-size, 0, 0, size);

//   // Lower Right

//   findMean(0, size, -size, 0);

//   fragColor = mean.r;
// }


#version 450




uniform sampler2D ssaoInput;
in vec2 TexCoords;

//uniform sampler2D texture;
//in vec2 v_texcoord;

uniform float sigmaS = 1.0;
uniform float sigmaL = 1.0;

uniform float facS = 1.0;
uniform float facL = 1.0;

//out vec4 FragColor;
out float FragColor;

float lum(in vec4 color) {
    return length(color.xyz);
}

void main()
{
  float sumW = 0.;
  float sumC = 0.;
  int halfSize = int(sigmaS) * 2;
  vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));

  float l = lum(texture(ssaoInput,TexCoords));

  for (int i = -halfSize; i <= halfSize; i ++){
    for (int j = -halfSize; j <= halfSize; j ++){
      vec2 pos = vec2(float(i), float(j));
      vec2 offset = pos * texelSize;
      vec4 offsetColor = texture(ssaoInput, TexCoords + offset);

      float distS = length(pos);
      float distL = lum(offsetColor)-l;

      float wS = exp(facS*float(distS*distS));
      float wL = exp(facL*float(distL*distL));
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