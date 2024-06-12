
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec4 weight;
layout (location = 4) in ivec4 indices;

const int MAX_JOINTS = 50;

out vec2 TexCoord;
out vec3 Normal;
out vec4 Color;

uniform mat4 view;
uniform mat4 jointTransforms[MAX_JOINTS];

void main()
{
  vec4 local_pos = vec4(0);
  vec4 color = vec4(1.0, 1.0f, 1.0, 1.0);
  for(int i = 0; i < 4; i++){
    int index             = indices[i];
    mat4 joint_transform  = jointTransforms[index];
    vec4 pose_position    = joint_transform * vec4(aPos, 1.0);
    local_pos            += pose_position * weight[i];
    if(i < 3){
      color[i] = (5 * index) / 255.0;
    }
  }

  gl_Position = view * local_pos;

  TexCoord = aTexCoord;
  Normal = aNormal;
  Color = color;
}
