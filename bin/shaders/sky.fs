#version 430

in vec3 pos;
in vec3 worldPos;
uniform mat4 projection;
uniform mat4 view;

uniform float time = 0.0;
uniform float cirrus = 0.0;
uniform float cumulus = 0.0;

uniform float Br = 0.0050;
uniform float Bm = 0.0015;
uniform float g =  0.9200;
vec3 nitrogen = vec3(0.650, 0.570, 0.475);
vec3 Kr = Br / pow(nitrogen, vec3(4.0));
vec3 Km = Bm / pow(nitrogen, vec3(0.84));

uniform vec3 sun_direction;

float hash(float n)
{
    return fract(sin(n) * 43758.5453123);
}

float noise(vec3 x)
{
    vec3 f = fract(x);
    float n = dot(floor(x), vec3(1.0, 157.0, 113.0));
    return mix(mix(mix(hash(n +   0.0), hash(n +   1.0), f.x),
                mix(hash(n + 157.0), hash(n + 158.0), f.x), f.y),
            mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
                mix(hash(n + 270.0), hash(n + 271.0), f.x), f.y), f.z);
}

const mat3 m = mat3(0.0, 1.60,  1.20, -1.6, 0.72, -0.96, -1.2, -0.96, 1.28);
float fbm(vec3 p)
{
    float f = 0.0;
    f += noise(p) / 2; p = m * p * 1.1;
    f += noise(p) / 4; p = m * p * 1.2;
    f += noise(p) / 6; p = m * p * 1.3;
    f += noise(p) / 12; p = m * p * 1.4;
    f += noise(p) / 24;
    return f;
}

// void main() {

//     // // Atmosphere Scattering
//     // float mu = dot(normalize(pos), normalize(sun_direction));
//     // float rayleigh = 3.0 / (8.0 * 3.14) * (1.0 + mu * mu);
//     // vec3 mie = (Kr + Km * (1.0 - g * g) / (2.0 + g * g) / pow(1.0 + g * g - 2.0 * g * mu, 1.5)) / (Br + Bm);

//     // vec3 day_extinction = exp(-exp(-((pos.y + sun_direction.y * 4.0) * (exp(-pos.y * 16.0) + 0.1) / 80.0) / Br) * (exp(-pos.y * 16.0) + 0.1) * Kr / Br) * exp(-pos.y * exp(-pos.y * 8.0 ) * 4.0) * exp(-pos.y * 2.0) * 4.0;
//     // vec3 night_extinction = vec3(1.0 - exp(sun_direction.y)) * 0.2;
//     // vec3 extinction = mix(day_extinction, night_extinction, -sun_direction.y * 0.2 + 0.5);
//     // color.rgb = rayleigh * mie * extinction;

//     // // Cirrus Clouds
//     // float density = smoothstep(1.0 - cirrus, 1.0, fbm(pos.xyz / pos.y * 2.0 + time * 0.05)) * 0.3;
//     // color.rgb = mix(color.rgb, extinction * 4.0, density * max(pos.y, 0.0));

//     // //Cumulus Clouds
//     // for (int i = 0; i < 3; i++)
//     // {
//     //     float density = smoothstep(1.0 - cumulus, 1.0, fbm((0.7 + float(i) * 0.01) * pos.xyz / pos.y + time * 0.3));
//     //     color.rgb = mix(color.rgb, extinction * density * 5.0, min(density, 1.0) * max(pos.y, 0.0));
//     // }

//     // // Dithering Noise
//     // color.rgb += noise(pos * 1000) * 0.01;

//     // Define the color of the plane
//     //vec3 planeColor = vec3(0.2, 0.8, 0.2);  // Adjust the RGB values as needed
//     // Find the world space position of the origin (0, 0, 0)
//     // vec4 worldSpaceOrigin = inverse(view) * vec4(0.0, 0.0, 0.0, 1.0);
//     // vec4 planeWorldSpaceOrigin = inverse(view) * worldSpaceOrigin;
//     // // Output color based on the world space position of the origin
//     // //color = vec4(worldSpaceOrigin.xyz, 1.0);

//     // vec3 planePos = worldPos - pos;
//     // float tX = worldPos.x;
//     // float tZ = worldPos.z;
    
//     // if (tX > -50.5 && tX < 50.5 && tZ > -50.5 && tZ < 50.5){
//     //     if (worldPos.y - pos.y < 0.0){
//     //         if (worldPos.y >= 0.0) {
//     //             color = vec4(1.0,1.0,1.0, 0.5);
//     //         } 
//     //         else {
//     //             color = vec4(0.0, 0.0, 0.0, 0.0);
//     //         }
//     //     }
//     //     else{
//     //         if (worldPos.y <= 0.0) {
//     //             color = vec4(1.0,1.0,1.0, 0.5);
//     //         } 
//     //         else {
//     //             color = vec4(0.0, 0.0, 0.0, 0.0);
//     //         }
//     //     }
//     // }
//     // else{
//     //     color = vec4(0.0, 0.0, 0.0, 0.0);
//     // }
    
//     // vec4 coords = projection * view * vec4(0.0,0.0,0.0,0.0);

//     // Draw grid lines on the plane


//         // Background color for other fragments
//        //color = vec4(worldPos, 1.0);
    

//     // vec4 clipSpace = vec4(gl_FragCoord.xy, 0.0, 1.0);
//     // vec4 eyeSpace = inverse(projection) * clipSpace;
//     // eyeSpace.z = -1.0;
//     // eyeSpace.w = 0.0;
//     // vec4 worldPos = (inverse(view) * eyeSpace);

//     // color = worldPos;
// }


in vec3 nearPoint;
in vec3 farPoint;
out vec4 colorRes;
float near = 0.01; //0.01
float far = 100.0; //100
vec4 grid(vec3 fragPos3D, float scale, bool drawAxis) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    vec4 color = vec4(0.2, 0.2, 0.2, 1.0 - min(line, 1.0));
    // z axis
    if(fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx)
        color.z = 1.0;
    // x axis
    if(fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz)
        color.x = 1.0;
    return color;
}

float computeDepth(vec3 pos) {
    vec4 clip_space_pos = projection * view * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

float computeLinearDepth(vec3 pos) {
    vec4 clip_space_pos = projection * view * vec4(pos.xyz, 1.0);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0; // put back between -1 and 1
    float linearDepth = (2.0 * near * far) / (far + near - clip_space_depth * (far - near)); // get linear value between 0.01 and 100
    return linearDepth / far; // normalize
}

void main() {
    float t = -nearPoint.y / (farPoint.y - nearPoint.y);
    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);

    gl_FragDepth = computeDepth(fragPos3D);

    float linearDepth = computeLinearDepth(fragPos3D);
    float fading = max(0, (0.5 - linearDepth));

    colorRes = (grid(fragPos3D, 10, true) + grid(fragPos3D, 1, true))* float(t > 0); // adding multiple resolution for the grid
    colorRes.a *= fading;
}