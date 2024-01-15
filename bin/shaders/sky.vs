#version 430

layout (location = 0) in vec3 aPos;

out vec3 pos;
out vec3 worldPos;
uniform mat4 projection;
uniform mat4 view;



out vec3 nearPoint;
out vec3 farPoint;

// Grid position are in clipped space
vec3 gridPlane[6] = vec3[](
    vec3(1000, 0.0, 1000), vec3(-1000, 0.0, -1000), vec3(-1000, 0.0, 1000),
    vec3(-1000, 0.0 , -1000), vec3(1000, 0.0, 1000), vec3(1000, 0.0, -1000)
);

vec3 UnprojectPoint(float x, float y, float z) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(projection);
    vec4 unprojectedPoint =  viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    vec3 p = gridPlane[gl_VertexID].xyz;
    nearPoint = UnprojectPoint(p.x, p.y, 0.0).xyz; // unprojecting on the near plane
    farPoint = UnprojectPoint(p.x, p.y, 1.0).xyz; // unprojecting on the far plane
    gl_Position = vec4(p, 1.0); // using directly the clipped coordinates

    //gl_Position = projection * view * vec4(p.xyz, 1.0);
    //gl_Position = projection * view * vec4(p, 1.0);
}
