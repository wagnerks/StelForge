#version 420
layout (location = 0) out highp vec4 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;
layout (location = 3) out highp vec3 gViewPosition;
layout (location = 4) out vec4 gOutlines;
layout (location = 5) out vec4 gLights;


in vec2 TexCoords;
in highp vec3 FragPos;
in vec3 Normal;
in vec3 ViewPos;

out vec4 FragColor;

layout(std140, binding = 5) uniform SharedMatrices {
    mat4 projection;
    mat4 view;
    mat4 PV;
} matrices;

uniform sampler2D outline;

uniform vec3 cameraPos;
uniform float far;
uniform float near;

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
    float linearDepth = (2.0 * near * far) / (far + near - clip_space_depth * (far - near)); // get linear value between 0.01 and 100
    return linearDepth / far; // normalize
}

void main()
{ 
    gOutlines.b = 0.0f;
    gLights = vec4(0.0);
    FragColor = vec4(1.0);

    // store the fragment position vector in the first gbuffer texture
    gPosition.rgb = FragPos;
    gPosition.a = gl_FragCoord.z;
    // also store the per-fragment normals into the gbuffer
    gNormal.xyz = Normal;
    gNormal.a = gl_FragCoord.z / gl_FragCoord.w; //4 byte for depth buffer

    gViewPosition = ViewPos;
    gAlbedoSpec.a = 0.0;
    
    float distanceKoef = min(abs(cameraPos.y), far) / far; //0 is near, 1 is far
    
    vec4 color1 = grid(FragPos.xyz, 1.0);
    color1.a *= 1.0 - (distanceKoef) * 40.0;
    color1.a *= 0.8;
    
    vec4 color2 = grid(FragPos.xyz, 0.1);
    color2.a *= 1.0 - (distanceKoef) * 10.0;

    vec4 color3 = grid(FragPos.xyz, 0.01);

    //fragColor = color1;
    gAlbedoSpec.rgb = color1.rgb;
    gAlbedoSpec.rgb = mix(color2.rgb, gAlbedoSpec.rgb, 1.0);
    gAlbedoSpec.rgb = mix(color3.rgb, gAlbedoSpec.rgb, 1.0);
    gAlbedoSpec.rgb *= vec3(max(color1.a, max(color2.a, color3.a)));
    gAlbedoSpec.rgb *= 3.0;
    float linearDepth = computeLinearDepth(FragPos.xyz);
    float fading = max(0, (0.8 - linearDepth));
    gAlbedoSpec.rgb *= vec3(fading);
}
