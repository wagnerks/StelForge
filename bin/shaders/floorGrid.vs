#version 410

layout (location=0) in vec3 position;


uniform mat4 PVM;
uniform mat4 PV;
uniform mat4 model;
uniform vec3 cameraPos;
uniform float far;
uniform float near;

out vec4 worldPos;

mat4 scaleMatrix = mat4(
    far, 0.0,   0.0,   0.0,
    0.0,   far, 0.0,   0.0,
    0.0,   0.0,   far, 0.0,
    0.0,   0.0,   0.0,   1.0
);

void main() {
    worldPos = model * scaleMatrix * vec4(position, 1.0);

    gl_Position = PV * vec4(worldPos.x + cameraPos.x, worldPos.y, worldPos.z + cameraPos.z, worldPos.w);

    worldPos.xz = worldPos.xz + cameraPos.xz;
}