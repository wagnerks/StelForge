#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssao;

struct DirectionalLight {
    mat4 PV; //proj * view from light perspective matrix
    sampler2D shadowsMap;
    vec3 Position;
    vec2 textureSize;
};

const int MAX_SHADOWS_SIZE = 3;
uniform DirectionalLight DirLights[MAX_SHADOWS_SIZE];
uniform int shadowsCount;

struct Light {
    vec3 Position;
    vec3 Color;
    
    float Linear;
    float Quadratic;
    float Radius;
};

const int NR_LIGHTS = 1000;
uniform Light lights[NR_LIGHTS];
uniform int lightsCount = 0;
uniform vec3 viewPos;

struct CascadedShadow {
    sampler2DArrayShadow shadowMap;
    vec3 direction;
    vec3 color;
    vec2 texelSize;
};

uniform CascadedShadow cascadedShadow;
uniform int cascadeCount;   // number of frusta - 1

uniform float farPlane;

uniform mat4 view;

layout (std140, binding = 0) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[16];
};

uniform float cascadePlaneDistances[16];
uniform float bias = 0.001;

const int pcfCount = 4;
const float shadowForce = 0.01;

const vec2 poisson_disk[64] =
{
    vec2(-0.5119625f, -0.4827938f),
    vec2(-0.2171264f, -0.4768726f),
    vec2(-0.7552931f, -0.2426507f),
    vec2(-0.7136765f, -0.4496614f),
    vec2(-0.5938849f, -0.6895654f),
    vec2(-0.3148003f, -0.7047654f),
    vec2(-0.42215f, -0.2024607f),
    vec2(-0.9466816f, -0.2014508f),
    vec2(-0.8409063f, -0.03465778f),
    vec2(-0.6517572f, -0.07476326f),
    vec2(-0.1041822f, -0.02521214f),
    vec2(-0.3042712f, -0.02195431f),
    vec2(-0.5082307f, 0.1079806f),
    vec2(-0.08429877f, -0.2316298f),
    vec2(-0.9879128f, 0.1113683f),
    vec2(-0.3859636f, 0.3363545f),
    vec2(-0.1925334f, 0.1787288f),
    vec2(0.003256182f, 0.138135f),
    vec2(-0.8706837f, 0.3010679f),
    vec2(-0.6982038f, 0.1904326f),
    vec2(0.1975043f, 0.2221317f),
    vec2(0.1507788f, 0.4204168f),
    vec2(0.3514056f, 0.09865579f),
    vec2(0.1558783f, -0.08460935f),
    vec2(-0.0684978f, 0.4461993f),
    vec2(0.3780522f, 0.3478679f),
    vec2(0.3956799f, -0.1469177f),
    vec2(0.5838975f, 0.1054943f),
    vec2(0.6155105f, 0.3245716f),
    vec2(0.3928624f, -0.4417621f),
    vec2(0.1749884f, -0.4202175f),
    vec2(0.6813727f, -0.2424808f),
    vec2(-0.6707711f, 0.4912741f),
    vec2(0.0005130528f, -0.8058334f),
    vec2(0.02703013f, -0.6010728f),
    vec2(-0.1658188f, -0.9695674f),
    vec2(0.4060591f, -0.7100726f),
    vec2(0.7713396f, -0.4713659f),
    vec2(0.573212f, -0.51544f),
    vec2(-0.3448896f, -0.9046497f),
    vec2(0.1268544f, -0.9874692f),
    vec2(0.7418533f, -0.6667366f),
    vec2(0.3492522f, 0.5924662f),
    vec2(0.5679897f, 0.5343465f),
    vec2(0.5663417f, 0.7708698f),
    vec2(0.7375497f, 0.6691415f),
    vec2(0.2271994f, -0.6163502f),
    vec2(0.2312844f, 0.8725659f),
    vec2(0.4216993f, 0.9002838f),
    vec2(0.4262091f, -0.9013284f),
    vec2(0.2001408f, -0.808381f),
    vec2(0.149394f, 0.6650763f),
    vec2(-0.09640376f, 0.9843736f),
    vec2(0.7682328f, -0.07273844f),
    vec2(0.04146584f, 0.8313184f),
    vec2(0.9705266f, -0.1143304f),
    vec2(0.9670017f, 0.1293385f),
    vec2(0.9015037f, -0.3306949f),
    vec2(-0.5085648f, 0.7534177f),
    vec2(0.9055501f, 0.3758393f),
    vec2(0.7599946f, 0.1809109f),
    vec2(-0.2483695f, 0.7942952f),
    vec2(-0.4241052f, 0.5581087f),
    vec2(-0.1020106f, 0.6724468f),
};

vec2 vogel_disk_sample(uint sample_index, uint sample_count, float angle)
{
  const float golden_angle = 2.399963f; // radians
  float r                  = sqrt(sample_index + 0.5f) / sqrt(sample_count);
  float theta              = sample_index * golden_angle + angle;
  float sine, cosine;

  sine = sin(theta);
  cosine = cos(theta);
  return vec2(cosine, sine) * r;
}

float quick_hash(vec2 pos) {
	const vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
	return fract(magic.z * fract(dot(pos, magic.xy)));
}

int sc_directional_soft_shadow_samples = 0;

// float sample_directional_pcf_shadow(sampler2D shadow, vec2 shadow_pixel_size, vec4 coord) {
// 	vec2 pos = coord.xy;
// 	float depth = coord.z;

// 	//if only one sample is taken, take it from the center
// 	if (sc_directional_soft_shadow_samples == 0) {
// 		return textureProj(sampler2DShadow(shadow), vec4(pos, depth, 1.0));
// 	}
//     return 0.0;
// 	// mat2 disk_rotation;
// 	// {
// 	// 	float r = quick_hash(gl_FragCoord.xy) * 2.0 * M_PI;
// 	// 	float sr = sin(r);
// 	// 	float cr = cos(r);
// 	// 	disk_rotation = mat2(vec2(cr, -sr), vec2(sr, cr));
// 	// }

// 	// float avg = 0.0;

// 	// for (uint i = 0; i < sc_directional_soft_shadow_samples; i++) {
// 	// 	avg += textureProj(sampler2DShadow(shadow, shadow_sampler), vec4(pos + shadow_pixel_size * (disk_rotation * scene_data_block.data.directional_soft_shadow_kernel[i].xy), depth, 1.0));
// 	// }

// 	// return avg * (1.0 / float(sc_directional_soft_shadow_samples));
// }

float VectorToDepthValue(vec3 Vec)
{
    vec3 AbsVec = abs(Vec);
    float LocalZcomp = max(AbsVec.x, max(AbsVec.y, AbsVec.z));

    const float f = 500.0;
    const float n = 0.10;
    float NormZComp = (f+n) / (f-n) - (2*f*n)/(f-n)/LocalZcomp;
    return (NormZComp + 1.0) * 0.5;
}


float Technique_Vogel(int layer, float bias, vec3 projCoords) {
    float shadow          = 0.0f;
    float penumbra        = 1.0f;
    int g_shadow_samples = 3;
    float temporal_offset = 0.0;
    float temporal_angle  = temporal_offset * 3.14 * 2;
    float g_shadow_filter_size = 3.0;

    if (layer > 2){
        g_shadow_samples = 1;
    }


    float g_shadow_samples_rpc = 1.0 / g_shadow_samples;
    for (uint i = 0; i < g_shadow_samples; i++) {
        vec2 offset = vogel_disk_sample(i, g_shadow_samples, temporal_angle) * cascadedShadow.texelSize * g_shadow_filter_size * penumbra;
        
        float pcfDepth = texture(cascadedShadow.shadowMap, vec4(projCoords.xy + offset, layer, projCoords.z));
        shadow += (projCoords.z + bias) > pcfDepth ? 1.0 : 0.0;   
    } 

    return shadow * g_shadow_samples_rpc;
}

// float Technique_PCF(Light light, float bias, vec3 projCoords, float texelSize) {
//     float shadow = 0.0f;
//     for(int x = -pcfCount; x <= pcfCount; ++x) {
//         for(int y = -pcfCount; y <= pcfCount; ++y) {
//             float pcfDepth = texture(light.shadowsMap, projCoords.xy + vec2(x, y) * texelSize).r; 
//             shadow += projCoords.z - bias > pcfDepth  ? 1.0 : 0.0;        
//         }  
//     }

//     return shadow;
// }

float ShadowCalculation(DirectionalLight light, vec3 projCoords, vec3 FragPos, vec3 lightDir, vec3 normal) {
    float vertexNormalToLight = dot(normal, lightDir);
    float normalKoef = 1 - (vertexNormalToLight + 1.0) * 0.5;
    if (normalKoef <= 0.0){
        return 1.0;
    }

    if (projCoords.z < 0.0){
        return 0.0;
    }
    float shadow = 0.f; 
    
    // PCF
    for(int y = -pcfCount; y <= pcfCount; ++y) { //pcf only for y axis
        float pcfDepth = texture(light.shadowsMap, projCoords.xy + vec2(0.f, y) * cascadedShadow.texelSize).r; 
        shadow += projCoords.z - bias > pcfDepth  ? 1.0 : 0.0;        
    }    

    return normalKoef * shadow * shadowForce;
}

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}


float ShadowCascadedCalculation(vec3 fragPosWorldSpace, vec3 Normal, float curShadow) {
    float vertexNormalToLight = dot(Normal, cascadedShadow.direction);
    float koef = 1 - (vertexNormalToLight + 1.0) * 0.5; //1 when parallel, 0.5 if normal, 0 if divergent
    if (koef <= 0.0){
        return 1.0;
    }
    // select cascade layer
    int layer = 0;
    vec4 fragPosLightSpace;
    vec3 projCoords;

    float maxX = 0.997;
    float minX = 0.003;

    for (; layer < cascadeCount; ++layer) {
        fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorldSpace, 1.0);
        projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w; // perform perspective divide
        projCoords = projCoords * 0.5 + 0.5; // transform to [0,1] range
       
        if (projCoords.x > minX && projCoords.x < maxX && projCoords.y > minX && projCoords.y < maxX && projCoords.z >= 0.0 && projCoords.z <= 0.5){
            break;
        }
    }
    
    float shadow = Technique_Vogel(layer, bias, projCoords); 
    return min(koef, shadow);
}

vec3 calculateLightDiffuse(vec3 Normal, vec3 lightDirection, vec3 Diffuse, vec3 lightColor){
    return max(dot(Normal, lightDirection), 0.0) * Diffuse * lightColor;
}

vec3 calculateSpecular(vec3 Normal, vec3 lightDirection, vec3 viewDirection, vec3 lightColor, float Specular){
    return pow(max(dot(Normal, normalize(lightDirection + viewDirection)), 0.0), 64.0) * lightColor * Specular;
}

vec3 calculateLighting(vec3 Normal, vec3 lightDirection, vec3 viewDirection, vec3 Diffuse, vec3 lightColor, float Specular){
    return calculateLightDiffuse(Normal, -lightDirection, Diffuse, lightColor) + calculateSpecular(Normal, -lightDirection, viewDirection, lightColor, Specular);
}

void main() {
    // retrieve data from gbuffer
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;
    float AmbientOcclusion = texture(ssao, TexCoords).r;

    float shadow = 0.f;
    vec3 lighting = vec3(Diffuse * 0.3f * AmbientOcclusion);
    
    vec3 viewDir  = normalize(viewPos - FragPos);

    shadow += ShadowCascadedCalculation(FragPos, Normal, 0.f);


    lighting += calculateLighting(Normal, cascadedShadow.direction, viewDir, Diffuse, cascadedShadow.color, Specular) * 0.7f; 
 
    for (int i = 0; i < shadowsCount; i++) {
        vec4 shadowCoord = DirLights[i].PV * vec4(FragPos,1.0);
        vec3 projCoords = shadowCoord.xyz / shadowCoord.w * 0.5 + 0.5;        
        vec3 lightDir = normalize(FragPos - DirLights[i].Position);
        lighting += calculateLighting(Normal, lightDir, viewDir, Diffuse, cascadedShadow.color, Specular) * 0.1; 
        if (lighting.x == 0.f && lighting.y == 0.f && lighting.z == 0.f){
            continue; //if here is fully shadowed object, why need i calculate shadow?
        }
        shadow += ShadowCalculation(DirLights[i], projCoords, FragPos, lightDir, Normal);

    }

    int lightsCount = lightsCount > NR_LIGHTS ? NR_LIGHTS : lightsCount;
    if (lightsCount > 0){
        for(int i = 0; i < lightsCount; ++i) {
            // calculate distance between light source and current fragment
            float distance = length(lights[i].Position - FragPos);
            if(distance < lights[i].Radius) {
                vec3 lightDir = normalize(lights[i].Position - FragPos);

                float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);
                
                lighting += (calculateLightDiffuse(Normal, lightDir, Diffuse, lights[i].Color) + calculateSpecular(Normal, lightDir, viewDir, lights[i].Color, Specular)) * attenuation;
            }
        }
    }
    
    
    lighting *= (1-shadow);
    
    
    
    gl_FragColor = vec4(lighting , 1.0);
}