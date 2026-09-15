#version 450 core
#include "sfe.glsl"

// Constants baked into the shader rather than pushed: the test is about which output location
// reaches which attachment, and a constant per location cannot be reordered by the caller the
// way push-block fields could. Four of them: the g-buffer writes that many (the last is the
// motion vectors), and three used to be where the count stopped being exercised.
layout(location = 0) out vec4 target0;
layout(location = 1) out vec4 target1;
layout(location = 2) out vec4 target2;
layout(location = 3) out vec4 target3;

void main() {
    target0 = vec4(20.0, 40.0, 60.0, 80.0) / 255.0;
    target1 = vec4(100.0, 120.0, 140.0, 160.0) / 255.0;
    target2 = vec4(180.0, 200.0, 220.0, 240.0) / 255.0;
    target3 = vec4(15.0, 35.0, 55.0, 75.0) / 255.0;
}
