#version 400 core

layout (location = 0) out vec4 FragColor;

in vec3 v_Normal;
in vec2 v_TexCoord;
in vec4 v_Vertex;
in vec4 v_LightSpacePos;

uniform int		  u_ShadowOnly;
uniform float	  u_ShadowBias;
uniform int		  u_UsePCF;

uniform int		  u_EVSMMode; // 0 - EVSM2, 1 - EVSM4

uniform float	  u_VarianceMinLimit;
uniform float	  u_LightBleedReduction;

uniform vec2      u_Exponents;

uniform vec4      u_LightPosition;
uniform sampler2D u_SceneTexture;

uniform sampler2D u_DepthMapTexture;

float calcShadow(vec4 fragLightSpacePos);
float chebyshev(vec2 moments, float depth);
float reduceLightBleed(float p_max, float amount);
float linstep(float minVal, float maxVal, float val);

void main() {
    vec3 N = normalize(v_Normal);
    vec3 L = normalize(u_LightPosition.xyz - v_Vertex.xyz);
    float NdotL = max(dot(N, L), 0.0);
    vec4 color = texture(u_SceneTexture, v_TexCoord) * NdotL;

	float shadow = calcShadow(v_LightSpacePos);

	if (u_ShadowOnly == 1) {
		FragColor = vec4(shadow);
	} else {
		FragColor = color * vec4(shadow);
	}
}



float calcShadow(vec4 fragLightSpacePos) {
	
	vec3 projCoords = fragLightSpacePos.xyz / fragLightSpacePos.w;

	// compute pos and neg after bias adjustment
	//float pos = exp(u_Exponents.x * projCoords.z);
	//float neg = -exp(-u_Exponents.y * projCoords.z);

	//float bias = 0.0;
	projCoords.z -= u_ShadowBias; // z bias
	projCoords = projCoords * 0.5 + vec3(0.5);

	//if (projCoords.z >= 1.0) {
	//	return 1.0;
	//}
	float shadow = 0.0;

	vec4 moments = texture(u_DepthMapTexture, projCoords.xy); // pos, pos^2, neg, neg^2

	projCoords = projCoords* 2.0 - 1.0;

	float pos = exp(u_Exponents.x * projCoords.z);
	float neg = -exp(-u_Exponents.y * projCoords.z);

		
	if (u_EVSMMode == 0) {
		shadow = chebyshev(moments.xy, pos);
		return shadow;
	} else {
		float posShadow = chebyshev(moments.xy, pos);
		float negShadow = chebyshev(moments.zw, neg);
		shadow = min(posShadow, negShadow);
		return shadow;
	}
}

float chebyshev(vec2 moments, float depth) {

	if (depth <= moments.x) {
		return 1.0;
	}

	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, u_VarianceMinLimit / 1000.0);

	float d = depth - moments.x; // attenuation
	float p_max = variance / (variance + d * d);

	return reduceLightBleed(p_max, u_LightBleedReduction);
}

float reduceLightBleed(float p_max, float amount) {
	return linstep(amount, 1.0, p_max);
}

float linstep(float minVal, float maxVal, float val) {
	return clamp((val - minVal) / (maxVal - minVal), 0.0, 1.0);
}