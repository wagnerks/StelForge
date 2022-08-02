#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4  model;
uniform mat4  lightSpaceMatrix;


out vec4 v_LightSpacePos;

void main(void) {
	v_LightSpacePos = lightSpaceMatrix * model * vec4(aPos,1.0);
	gl_Position = v_LightSpacePos;
}
