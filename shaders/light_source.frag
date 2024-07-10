#version 450 core



uniform sampler2D texture1;

out vec4 FragColor;

void main()
{

  FragColor = vec4(1.0, 1.0, 1.0, 1.0) + texture(texture1, vec2(0.2, 0.2)) * 0.001;

}
