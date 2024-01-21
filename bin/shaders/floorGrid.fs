#version 410

in vec4 worldPos;

out vec4 fragColor;

uniform vec3 cameraPos;
uniform mat4 PV;
uniform float far;
uniform float near;

vec4 grid(vec3 fragPos3D, float scale) {
    vec2 coord = fragPos3D.xz * scale; // use the scale variable to set the distance between the lines
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    vec4 color = vec4(0.2, 0.2, 0.2, 1.2 - min(line, 1.0));
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
    vec4 clip_space_pos = PV * vec4(pos.xyz, 1.0);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0; // put back between -1 and 1
    float linearDepth = (2.0 * near * far) / (far + near - clip_space_depth * (far - near)); // get linear value between 0.01 and 100
    return linearDepth / far; // normalize
}

void main()
{
    float distanceKoef = min(abs(cameraPos.y), far) / far; //0 is near, 1 is far
    float linearDepth = computeLinearDepth(worldPos.xyz);
    float fading = max(0, (0.8 - linearDepth));
    //vec4 color1 = grid(worldPos.xyz, 10.0);
    //color1.a *= 1.0 - (distanceKoef) * 400.0;
    vec4 color2 = grid(worldPos.xyz, 1.0);
    color2.a *= 1.0 - (distanceKoef) * 40.0;
    color2.a *= 0.8;
    
    vec4 color3 = grid(worldPos.xyz, 0.1);
    color3.a *= 1.0 - (distanceKoef) * 10.0;

    vec4 color4 = grid(worldPos.xyz, 0.01);

    //fragColor = color1;
    fragColor = color2;
    fragColor += color3;
    fragColor += color4;
    fragColor.a = max(color2.a, max(color3.a, color4.a));

    fragColor.a *= fading;
}