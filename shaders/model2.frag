#version 450 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 normal;
in vec3 FragPos;
in vec4 FragPosLightSpace;

uniform sampler2D shadow_map;
uniform samplerCube shadow_map_cube;
uniform sampler2D texture1;
uniform vec3      light_position;
uniform vec3      ambient_lighting;
uniform vec3      viewPos;
uniform vec3      directional_light_direction;
uniform float far_plane;

float shadow_calc_directional_light(vec4 pos){
  vec3 proj_coords = pos.xyz / pos.w;

  proj_coords = proj_coords * 0.5 + 0.5;
  float closest_depth = texture(shadow_map, proj_coords.xy).r;
  float current_depth = proj_coords.z;

  float bias = max(0.005 * (1.0 - dot(normal, -directional_light_direction * 100)), 0.005);

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

float shadow_calc_point_light(vec3 pos){
  
  vec3 fragToLight = pos - light_position;
  float closest_depth = texture(shadow_map_cube, fragToLight).r;
  closest_depth *= far_plane;
  float current_depth = length(fragToLight);

  float bias = 0.05;
  float shadow = current_depth - bias > closest_depth ? 1.0 : 0.0;
  return shadow * 0.001;
}

vec3 calculate_point_light(vec3 norm, vec3 viewDir){
  vec3 lightDir = normalize(light_position - FragPos);
  float diff = max(dot(normal, lightDir), 0.0);
  vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);

  float specularStrength = 0.5;
  vec3 reflectDir = reflect(-lightDir, norm);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
  vec3 specular = specularStrength * spec * vec3(1.0, 1.0, 1.0);

  float shadow = shadow_calc_point_light(FragPos);

  vec3 c = (ambient_lighting + (1.0 - shadow) * (diffuse + specular)) * texture(texture1, TexCoord).rgb;
  return c;

}

vec3 calculate_directional_light(vec3 norm, vec3 viewDir){
  vec3 lightDir = normalize(-directional_light_direction);

  float diff = max(dot(normal, lightDir), 0.0);

  vec3 reflectDir = reflect(-lightDir, norm);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);


  vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);
  float specular_strength = 0.5;
  vec3 specular = specular_strength * spec * vec3(1.0, 1.0, 1.0);

  float shadow = shadow_calc_directional_light(FragPosLightSpace);

  vec3 c = (ambient_lighting + (1.0 - shadow) * (diffuse + specular)) * texture(texture1, TexCoord).rgb;
  return c;

}

void main()
{

  vec3 norm = normalize(normal);
  vec3 viewDir = normalize(viewPos - FragPos);

  vec3 point_light        = calculate_point_light(norm, viewDir);
  vec3 directional_light  = calculate_directional_light(norm, viewDir);

  FragColor = vec4(point_light + directional_light, 1.0);

  

}
