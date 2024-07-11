#version 450 core


out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{
  vec4 c = texture(skybox, TexCoords);
  if(c.x != 0 || c.y != 0 || c.z != 0){
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
  }else{
    FragColor = c;
  }
}
