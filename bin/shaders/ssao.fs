#version 430 core

out float FragColor;

in vec2 TexCoords;

layout(binding = 0) uniform sampler2D gPosition;
layout(binding = 1) uniform sampler2D gNormal;
//layout(binding = 2) uniform sampler2D noiseTexture;

layout(std140, binding = 3) uniform SSAOParams {
    float samples;
    float intensity;
    float scale;
    float bias;
    float sample_rad;
    float max_distance;
} params;


#define MOD3 vec3(.1031,.11369,.13787)


float hash12(vec2 p)
{
	vec3 p3  = fract(vec3(p.xyx) * MOD3);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 hash22(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * MOD3);
    p3 += dot(p3, p3.yzx+19.19);
    return fract(vec2((p3.x + p3.y)*p3.z, (p3.x+p3.z)*p3.y));
}

vec3 getPosition(vec2 uv) {
    float fl = 1.0 - texture(gNormal, uv).w;
    float d = texture(gNormal, uv).w;
       
    vec2 p = uv*2.-1.;
    mat3 ca = mat3(1.,0.,0.,   0.,1.,0.,   0.,0.,-1./1.5);

    vec3 rd = normalize( ca * vec3(p,fl) );
    
	vec3 pos = rd * d;
    return pos;
}

vec3 getNormal(vec2 uv) {
    return texture(gNormal, uv, 0.).xyz;
}

vec2 getRandom(vec2 uv) {
    return normalize(hash22(uv*126.1231) * 2. - 1.);
}

float doAmbientOcclusion(in vec2 tcoord,in vec2 uv, in vec3 p, in vec3 cnorm)
{
    vec3 diff = getPosition(tcoord + uv) - p;
    float l = length(diff);
    vec3 v = diff/l;
    float d = l*params.scale;
    float ao = max(0.0,dot(cnorm,v)-params.bias)*(1.0/(1.0+d));
    ao *= smoothstep(params.max_distance,params.max_distance * 0.5, l);
    return ao;
}

float spiralAO(vec2 uv, vec3 p, vec3 n, float rad)
{
    float goldenAngle = 2.4;
    float ao = 0.;
    float inv = 1. / params.samples;
    float radius = 0.;

    float rotatePhase = hash12( uv*100. ) * 6.28;
    float rStep = inv * rad;
    vec2 spiralUV;

    for (int i = 0; i < int(params.samples); i++) {
        spiralUV.x = sin(rotatePhase);
        spiralUV.y = cos(rotatePhase);
        radius += rStep;
        ao += doAmbientOcclusion(uv, spiralUV * radius, p, n);
        rotatePhase += goldenAngle;
    }
    ao *= inv;
    return ao;
}

void main() {
    vec3 p = getPosition(TexCoords);
    vec3 n = getNormal(TexCoords);

    float rad = params.sample_rad/p.z;
    float ao = spiralAO(TexCoords, p, n, rad);

    ao = 1. - ao * params.intensity;
    
	FragColor = ao;
}