#version 450 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 normal;
in vec3 FragPos;

uniform sampler2D texture1;
uniform vec3 light_position;
uniform vec3 ambient_lighting;

void main()
{

   float diff = clamp(dot(normal, vec3(normalize(light_position - FragPos))), 0.0, 1.0);  

   vec3 diffuse = vec3(1.0, 1.0, 1.0) * diff;
   FragColor = vec4((ambient_lighting + diffuse), 1.0) * texture(texture1, TexCoord);
}
