
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec4 weight;
layout (location = 4) in ivec4 indices;
layout (location = 5) in vec3 tangent;
layout (location = 6) in vec3 bitangent;

const int MAX_JOINTS = 100;

out vec2 TexCoord;
out vec3 FragPos;
out vec3 TangentLightPos;
out vec3 TangentViewPos;
out vec3 TangentFragPos;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 light_space_matrix;
uniform mat4 jointTransforms[MAX_JOINTS];
uniform vec3      light_position;
uniform vec3      viewPos;

void main()
{
  vec4 local_pos = vec4(0);
  for(int i = 0; i < 4; i++){
    int index             = indices[i];
    mat4 joint_transform  = jointTransforms[index];
    vec4 pose_position    = vec4(aPos, 1.0) * joint_transform;
    local_pos            += pose_position * weight[i];
  }

  gl_Position = local_pos * model * view * projection;

  TexCoord          = aTexCoord;
  FragPos           = vec3(vec4(aPos, 1.0) * model);
  FragPosLightSpace =  vec4(FragPos, 1.0) * light_space_matrix;

  mat3 normal_mat = mat3(transpose(inverse(model)));
  vec3 T          = normalize(normal_mat * tangent);
  vec3 N          = normalize(normal_mat * aNormal);
  T               = normalize(T - dot(T, N) * N);
  vec3 B          = cross(N, T);

  mat3 TBN = transpose(mat3(T, B, N));
  TangentLightPos = TBN * light_position;
  TangentViewPos = TBN * viewPos;
  TangentFragPos = TBN * FragPos;


}
