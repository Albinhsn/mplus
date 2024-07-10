#version 450 core


uniform samplerCube texture1;

in vec3 pos;
out vec4 FragColor;

void main()
{
  FragColor = texture(texture1, pos) + vec4(0.1, 0, 0, 1);

}
