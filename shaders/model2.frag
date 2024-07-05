#version 450 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 normal;
in vec3 FragPos;
in vec4 FragPosLightSpace;

uniform sampler2D shadow_map;
uniform sampler2D texture1;
uniform vec3      light_position;
uniform vec3      ambient_lighting;

float shadow_calc(vec4 pos){
  vec3 proj_coords = pos.xyz / pos.w;

  proj_coords = proj_coords * 0.5 + 0.5;
  float closest_depth = texture(shadow_map, proj_coords.xy).r;
  float current_depth = proj_coords.z;

  float bias = max(0.005 * (1.0 - dot(normal, -light_position)), 0.005);
  float shadow = current_depth - bias > closest_depth ? 1.0 : 0.0;

  return shadow;
}

void main()
{

   float diff = clamp(dot(normal, vec3(normalize(light_position - FragPos))), 0.0, 1.0);  

   vec3 diffuse = vec3(1.0, 1.0, 1.0) * diff;

   float shadow = shadow_calc(FragPosLightSpace);
   vec3 c = (ambient_lighting + (1.0 - shadow )) * diffuse * texture(texture1, TexCoord).rgb;
   FragColor = vec4(c, 1.0);
}
