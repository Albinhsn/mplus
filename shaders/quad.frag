#version 450 core


uniform sampler2D  texture1;

in vec2 pos;
out vec4 FragColor;

void main()
{
  FragColor = texture(texture1, pos);

}
