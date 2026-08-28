#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sourceTex;

void main() {
    outColor = vec4(texture(sourceTex, vUV).rgb, 1.0);
}
