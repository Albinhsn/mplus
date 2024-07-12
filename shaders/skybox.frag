#version 450 core


out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{
  vec4 c = texture(skybox, TexCoords);
  FragColor = vec4(c.r, c.g, 0, 1);
}
