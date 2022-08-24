#version 330

layout (location = 0) in vec3 aPos;

  out vec3 pos;

  uniform mat4 projection;
  uniform mat4 view;

  void main()
  {
    gl_Position = vec4(aPos.xy, 1.0, 1.0);
    pos = transpose(mat3(view)) * (inverse(projection) * gl_Position).xyz;
  }
