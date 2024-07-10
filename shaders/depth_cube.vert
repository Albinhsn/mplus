#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aTexCoord;
layout (location = 2) in vec3 aNormal;

uniform mat4 model;


void main()
{
  gl_Position =  vec4(aPos, 1.0) * model;
}
