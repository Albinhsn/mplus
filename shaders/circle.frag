#version 450 core

out vec4 FragColor;

in vec2 TexCoord;


uniform float thickness;
uniform vec4 color;

void main()
{

  float r = length(vec2(TexCoord.x - 0.5, TexCoord.y - 0.5));

  float sd = r - 0.4;

  vec2 gradient = vec2(dFdx(sd), dFdy(sd));

  float distance_from_line = abs(sd/length(gradient));

  float line_weight = clamp(thickness - distance_from_line, 0.0f, 1.0f);
  FragColor = vec4(color.r, color.g, color.b, line_weight);
}
