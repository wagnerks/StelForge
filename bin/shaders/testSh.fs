#version 420
layout(location = 0) out vec4 gMaterial;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;
layout (location = 3) out highp vec3 gViewPosition;


in vec2 TexCoords;
in highp vec3 FragPos;
in vec3 Normal;
in vec3 ViewPos;

out vec4 FragColor;

layout(std140, binding = 5) uniform SharedMatrices {
    SFE_SHARED_MATRICES
} matrices;

layout(std140, binding = 15) uniform PushConstants {
    vec4 cameraPos;
    float far;
    float near;
} pc;

vec4 grid(vec3 fragPos3D, float scale) {
    vec2 coord = fragPos3D.xz * scale; // use the scale variable to set the distance between the lines
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    vec4 color = vec4(0.2, 0.2, 0.2, 2.5 - min(line, 1.0));
    // z axis
    if(fragPos3D.x > -0.5 * minimumx && fragPos3D.x < 0.5 * minimumx){
        color.z = 1.0;
        color.a = 1.0;
    }
        
    // x axis
    if(fragPos3D.z > -0.5 * minimumz && fragPos3D.z < 0.5 * minimumz){
        color.x = 1.0;
        color.a = 1.0;
    }
        
    
    return color;
}

float computeLinearDepth(vec3 pos) {
    vec4 clip_space_pos = matrices.PV * vec4(pos.xyz, 1.0);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0; // put back between -1 and 1
    float linearDepth = (2.0 * pc.near * pc.far) / (pc.far + pc.near - clip_space_depth * (pc.far - pc.near)); // get linear value between 0.01 and 100
    return linearDepth / pc.far; // normalize
}

void main()
{ 
    FragColor = vec4(1.0);

    gMaterial = vec4(0.0);
    gNormal.xyz = vec3(0.0, 1.0, 0.0);
    gNormal.a = gl_FragCoord.z / gl_FragCoord.w;

    gViewPosition = ViewPos;
    gAlbedoSpec.a = 0.28;
    
    float distanceKoef = min(abs(pc.cameraPos.y), pc.far) / pc.far; //0 is near, 1 is far
    
    vec4 color1 = grid(FragPos.xyz, 1.0);
    color1.a *= 1.0 - (distanceKoef) * 40.0;
    color1.a *= 0.8;
    
    vec4 color2 = grid(FragPos.xyz, 0.1);
    color2.a *= 1.0 - (distanceKoef) * 10.0;

    vec4 color3 = grid(FragPos.xyz, 0.01);

    float linearDepth = computeLinearDepth(FragPos.xyz);
    float fading = max(0, (0.8 - linearDepth));
    float lines = clamp(max(color1.a, max(color2.a, color3.a)), 0.0, 1.0);
    gAlbedoSpec.rgb = mix(vec3(0.12, 0.12, 0.13), vec3(0.04), lines * 0.7);
    gAlbedoSpec.rgb *= vec3(fading);
}
