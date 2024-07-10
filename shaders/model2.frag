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
uniform vec3      viewPos;

float shadow_calc(vec4 pos){
  vec3 proj_coords = pos.xyz / pos.w;

  proj_coords = proj_coords * 0.5 + 0.5;
  float closest_depth = texture(shadow_map, proj_coords.xy).r;
  float current_depth = proj_coords.z;

  float bias = max(0.005 * (1.0 - dot(normal, -light_position)), 0.005);

  float shadow = 0.0;
  vec2 texelSize = 1.0 / textureSize(shadow_map, 0);

  for(int x = -1; x <= 1; ++x){
    for(int y = -1; y <= 1; ++y){
      float pcfDepth = texture(shadow_map, proj_coords.xy + vec2(x, y) * texelSize).r;
      shadow += current_depth - bias > pcfDepth ? 1.0 : 0.0;
    }
  }

  shadow /= 9.0;
  shadow = proj_coords.z > 1.0 ? 0 : shadow;

  return shadow;
}

void main()
{

  vec3 norm = normalize(normal);
  vec3 lightDir = normalize(light_position - FragPos);
  float diff = max(dot(normal, lightDir), 0.0);
  vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);

  float specularStrength = 0.5;
  vec3 viewDir = normalize(viewPos - FragPos);
  vec3 reflectDir = reflect(-lightDir, norm);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
  vec3 specular = specularStrength * spec * vec3(1.0, 1.0, 1.0);

  float shadow = shadow_calc(FragPosLightSpace);

  vec3 c = (ambient_lighting + (1.0 - shadow) * (diffuse + specular)) * texture(texture1, TexCoord).rgb;
  FragColor = vec4(c, 1.0);

}
