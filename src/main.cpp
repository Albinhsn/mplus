#include "animation.h"
#include "files.h"
#include "sdl.h"
#include "shader.h"
#include "vector.h"
#include <GL/glext.h>
#include <SDL2/SDL_events.h>
#include <assimp/Importer.hpp>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <cassert>
#include <cfloat>
#include <cstdlib>

bool should_quit()
{
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    if (event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
    {
      return true;
    }
  }
  return false;
}

Quaternion Quaternion::from_mat(Mat44 m)
{
  float diagonal = m.rc[0][0] + m.rc[1][1] + m.rc[2][2];
  float x, y, z, w;
  if (diagonal > 0)
  {
    float w4 = sqrtf(diagonal + 1.0f) * 2.0f;
    x        = (m.rc[2][1] - m.rc[1][2]) / w4;
    y        = (m.rc[0][2] - m.rc[2][0]) / w4;
    z        = (m.rc[1][0] - m.rc[0][1]) / w4;
    w        = w4 / 4.0f;
  }
  else if (m.rc[0][0] > m.rc[1][1] && m.rc[0][0] > m.rc[2][2])
  {
    float x4 = sqrtf(1.0f + m.rc[0][0] - m.rc[1][1] - m.rc[2][2]) * 2.0f;
    x        = x4 / 4.0f;
    y        = (m.rc[0][1] + m.rc[1][0]) / x4;
    z        = (m.rc[0][2] + m.rc[2][0]) / x4;
    w        = (m.rc[2][1] - m.rc[1][2]) / x4;
  }
  else if (m.rc[1][1] > m.rc[2][2])
  {
    float y4 = sqrtf(1.0f + m.rc[1][1] - m.rc[0][0] - m.rc[2][2]) * 2.0f;
    x        = (m.rc[0][1] + m.rc[1][0]) / y4;
    y        = y4 / 4.0f;
    z        = (m.rc[1][2] + m.rc[2][1]) / y4;
    w        = (m.rc[0][2] - m.rc[2][0]) / y4;
  }
  else
  {
    float z4 = sqrtf(1.0f + m.rc[2][2] - m.rc[0][0] - m.rc[1][1]) * 2.0f;
    x        = (m.rc[0][2] + m.rc[2][0]) / z4;
    y        = (m.rc[1][2] + m.rc[2][1]) / z4;
    z        = z4 / 4.0f;
    w        = (m.rc[1][0] - m.rc[0][1]) / z4;
  }
  return Quaternion(x, y, z, w);
}

Mat44 Quaternion::to_matrix()
{
  f32   x_squared = this->x * this->x;
  f32   y_squared = this->y * this->y;
  f32   z_squared = this->z * this->z;
  f32   xy        = this->x * this->y;
  f32   xz        = this->x * this->z;
  f32   xw        = this->x * this->w;
  f32   yz        = this->y * this->z;
  f32   yw        = this->y * this->w;
  f32   zw        = this->z * this->w;

  Mat44 m         = {};
  m.rc[0][0]      = 1 - 2 * (y_squared + z_squared);
  m.rc[0][1]      = 2 * (xy - zw);
  m.rc[0][2]      = 2 * (xz + yw);
  m.rc[0][3]      = 0;
  m.rc[1][0]      = 2 * (xy + zw);
  m.rc[1][1]      = 1 - 2 * (x_squared + z_squared);
  m.rc[1][2]      = 2 * (yz - xw);
  m.rc[1][3]      = 0;
  m.rc[2][0]      = 2 * (xz - yw);
  m.rc[2][1]      = 2 * (yz + yw);
  m.rc[2][2]      = 1 - 2 * (x_squared + y_squared);
  m.rc[2][3]      = 0;
  m.rc[3][0]      = 0;
  m.rc[3][1]      = 0;
  m.rc[3][2]      = 0;
  m.rc[3][3]      = 1;

  return m;
}

Mat44 Mat44::rotate(Quaternion q)
{
  return this->mul(q.to_matrix());
}

Quaternion Quaternion::interpolate(Quaternion q0, Quaternion q1, f32 t0)
{
  float x, y, z, w;

  float dot = q0.w * q1.w + q0.x * q1.x + q0.y * q1.y + q0.z * q1.z;
  float t1  = 1.0f - t0;

  if (dot < 0)
  {
    x = t1 * q0.x + t0 * -q1.x;
    y = t1 * q0.y + t0 * -q1.y;
    z = t1 * q0.z + t0 * -q1.z;
    w = t1 * q0.w + t0 * -q1.w;
  }
  else
  {
    x = t1 * q0.x + t0 * q1.x;
    y = t1 * q0.y + t0 * q1.y;
    z = t1 * q0.z + t0 * q1.z;
    w = t1 * q0.w + t0 * q1.w;
  }
  float sum = sqrtf(x * x + y * y + z * z + w * w);
  x /= sum;
  y /= sum;
  z /= sum;
  w /= sum;

  return Quaternion(x, y, z, w);
}

Vector3 interpolate_translation(Vector3 v0, Vector3 v1, f32 t)
{
  float x = v0.x + (v1.x - v0.x) * t;
  float y = v0.y + (v1.y - v0.y) * t;
  float z = v0.z + (v1.z - v0.z) * t;

  return Vector3(x, y, z);
}

Mat44 interpolate_transforms(Mat44 first, Mat44 second, f32 time)
{
  Vector3    first_translation(first.rc[3][0], first.rc[3][1], first.rc[3][2]);
  Vector3    second_translation(second.rc[3][0], second.rc[3][1], second.rc[3][2]);
  Vector3    final_translation = interpolate_translation(first_translation, second_translation, time);

  Quaternion first_q           = Quaternion::from_mat(first);
  Quaternion second_q          = Quaternion::from_mat(second);
  Quaternion final_q           = Quaternion::interpolate(first_q, second_q, time);

  Mat44      res               = {};
  res.identity();
  res = res.translate(final_translation);
  res = res.rotate(final_q);

  return res;
}

void calculate_new_pose(Mat44* poses, u32 count, Animation animation, u32 ticks)
{
  u64   loop_time = 2000;
  float time      = (ticks % (u64)(loop_time * animation.duration)) / (f32)loop_time;

  u32   pose_idx  = 0;
  if (time >= animation.duration - 0.001f)
  {
    time = animation.duration;
  }

  for (; pose_idx < animation.pose_count && animation.steps[pose_idx] < time; pose_idx++)
    ;

  // get the two key frames
  JointPose* first              = &animation.poses[pose_idx - 1];
  JointPose* second             = &animation.poses[pose_idx];

  float      time_between_poses = (time - animation.steps[pose_idx - 1]) / (animation.steps[pose_idx] - animation.steps[pose_idx - 1]);

  // interpolate between the frames given the progression
  //  iterate over each joint
  for (u32 i = 0; i < animation.joint_count; i++)
  {
    //    grab the two transform and interpolate between them
    //    interpolate them
    Mat44 first_transform  = first->local_transform[i];
    Mat44 second_transform = second->local_transform[i];
    poses[i]               = interpolate_transforms(first_transform, second_transform, time_between_poses);
  }

  printf("%f\n", time);
  printf("%d %d\n", pose_idx, animation.pose_count);
}

void update_animation(AnimationModel animation, Shader shader, u32 ticks)
{
  Skeleton* skeleton = &animation.skeleton;
  Mat44     transforms[skeleton->joint_count];
  Mat44     current_poses[skeleton->joint_count];
  calculate_new_pose(current_poses, skeleton->joint_count, animation.animations, ticks);

  Mat44 parent_transforms[skeleton->joint_count];
  for (int i = 0; i < skeleton->joint_count; i++)
  {
    parent_transforms[i].identity();
  }

  for (u32 i = 0; i < skeleton->joint_count; i++)
  {
    Joint* joint             = &skeleton->joints[i];

    Mat44  current_local     = current_poses[i];
    Mat44  parent_transform  = parent_transforms[joint->m_iParent];

    Mat44  current_transform = parent_transform.mul(current_local);
    parent_transforms[i]     = current_transform;
    transforms[i]            = current_transform.mul(joint->m_invBindPose);
  }
  shader.set_mat4("jointTransforms", &transforms[0].m[0], skeleton->joint_count);
}

int main()
{

  AnimationModel animation = {};

  sta_collada_parse_from_file(&animation, "./data/model.dae");

  const int     screen_width  = 620;
  const int     screen_height = 480;

  SDL_GLContext context;
  SDL_Window*   window;
  sta_init_sdl_gl(&window, &context, screen_width, screen_height);

  ModelData model  = {};
  bool      result = sta_parse_wavefront_object_from_file(&model, "./data/unarmed_man/unarmed.obj");
  if (!result)
  {
    printf("Failed to read model!\n");
    return 1;
  }

  TargaImage image = {};
  result           = sta_read_targa_from_file_rgba(&image, "./data/unarmed_man/Peasant_Man_diffuse.tga");
  if (!result)
  {
    printf("Failed to read peasant man diffuse\n");
    return 1;
  }

  GLuint vao, vbo, ebo;
  sta_glGenVertexArrays(1, &vao);
  sta_glGenBuffers(1, &vbo);
  sta_glGenBuffers(1, &ebo);

  sta_glBindVertexArray(vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, vbo);
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(SkinnedVertex) * animation.vertex_count, animation.vertices, GL_STATIC_DRAW);

  sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * animation.vertex_count, animation.indices, GL_STATIC_DRAW);

  int size = sizeof(float) * 16;
  sta_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, size, (void*)0);
  sta_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, size, (void*)(3 * sizeof(float)));
  sta_glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, size, (void*)(5 * sizeof(float)));
  sta_glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, size, (void*)(8 * sizeof(float)));
  sta_glVertexAttribIPointer(4, 4, GL_INT, size, (void*)(12 * sizeof(int)));
  sta_glEnableVertexAttribArray(0);
  sta_glEnableVertexAttribArray(1);
  sta_glEnableVertexAttribArray(2);
  sta_glEnableVertexAttribArray(3);
  sta_glEnableVertexAttribArray(4);

  unsigned int texture0;
  sta_glGenTextures(1, &texture0);
  sta_glBindTexture(GL_TEXTURE_2D, texture0);

  sta_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data);
  sta_glGenerateMipmap(GL_TEXTURE_2D);

  Shader shader("./shaders/animation.vert", "./shaders/animation.frag");

  shader.use();
  shader.set_int("texture1", 0);

  glActiveTexture(GL_TEXTURE0);
  sta_glBindTexture(GL_TEXTURE_2D, texture0);
  sta_glBindVertexArray(vao);

  u32   ticks = 0;

  Mat44 view  = {};
  view.identity();
  view = view.scale(0.1f);
  while (true)
  {

    if (should_quit())
    {
      break;
    }
    if (ticks + 16 < SDL_GetTicks())
    {
      ticks = SDL_GetTicks() + 16;
      view  = view.rotate_y(0.5f);
      shader.set_mat4("view", &view.m[0]);
    }
    update_animation(animation, shader, ticks);

    sta_gl_clear_buffer(0.0f, 0.0f, 0.0f, 1.0f);

    glDrawElements(GL_TRIANGLES, animation.vertex_count, GL_UNSIGNED_INT, 0);
    sta_gl_render(window);
  }
}
