
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in int indices;
layout (location = 4) in vec3 weight;

out vec2 TexCoord;

uniform mat4 view;

void main()
{
  gl_Position = view * vec4(aPos, 1.0);

  TexCoord = aTexCoord;

}
