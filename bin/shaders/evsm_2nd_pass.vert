#version 330 core

in vec4 a_Vertex;
in vec3 a_Normal;
in vec2 a_TexCoord;

out vec3 v_Normal;
out vec2 v_TexCoord;
out vec4 v_Vertex;
out vec4 v_LightSpacePos;

uniform mat4  u_ModelViewMatrix;
uniform mat4  u_ProjectionMatrix;
uniform mat4  u_LightViewMatrix;		// Use these two matrixes to calculate vertex position in ...
uniform mat4  u_LightProjectionMatrix;  // ...light view space, or
uniform mat4  u_ShadowTransformMatrix;	// calculate transformation in app and pass it in this variable into shader
uniform int   u_UserVariableInt;
uniform float u_UserVariableFloat;


void main() {
    v_Vertex   = u_ModelViewMatrix * a_Vertex;
    v_Normal   = mat3(u_ModelViewMatrix) * a_Normal;
    v_TexCoord = a_TexCoord;

    // TODO: implement shadow generation 
    // 1. Compute vertex position in light view-space and store it in v_LightSpacePos
    //mat4 shadowTransform;
    //v_LightSpacePos = shadowTransform * a_Vertex;

	v_LightSpacePos = u_ShadowTransformMatrix * a_Vertex;

    gl_Position = u_ProjectionMatrix * v_Vertex;
}
