#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;


uniform mat4 view;
uniform mat4 model;
uniform mat4 projection;
uniform mat4 light_space_matrix;

out vec2 TexCoord;
out vec3 FragPos;
out vec3 normal;
out vec4 FragPosLightSpace;

void main()
{

  gl_Position = vec4(aPos, 1.0) *model * view * projection;

  TexCoord          = aTexCoord;
  FragPos           = vec3(vec4(aPos, 1.0) * model);
  FragPosLightSpace =  vec4(FragPos, 1.0) * light_space_matrix;
  normal            =  aNormal * mat3(transpose(inverse(model)));
}
