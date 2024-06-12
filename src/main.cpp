#include "animation.h"
#include "files.h"
#include "sdl.h"
#include "shader.h"
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

  Mat44  parent_transforms[animation.skeleton.joint_count];
  Mat44  transforms[animation.skeleton.joint_count];
  Mat44  current_poses[animation.skeleton.joint_count];

  for (u32 i = 0; i < animation.skeleton.joint_count; i++)
  {
    current_poses[i] = animation.animations.poses[0].local_transform[i];
    // printf("%.*s:\n", animation.animations.poses[0].names[i].l, animation.animations.poses[0].names[i].s);
    // current_poses[i].debug();
    // printf("---\n");
  }

  for (int i = 0; i < animation.skeleton.joint_count; i++)
  {
    parent_transforms[i].identity();
  }

  for (u32 i = 0; i < animation.skeleton.joint_count; i++)
  {
    Joint* joint             = &animation.skeleton.joints[i];

    Mat44  current_local     = current_poses[i];
    Mat44  parent_transform  = parent_transforms[joint->m_iParent];

    Mat44  current_transform = parent_transform.mul(current_local);
    parent_transforms[i]     = current_transform;
    transforms[i]            = current_transform.mul(joint->m_invBindPose);

    printf("Set %.*s to:\n", joint->m_name_length, joint->m_name);
    printf("Inv bind:\n");
    joint->m_invBindPose.debug();
    printf("---\n");
    printf("Current local:\n");
    current_local.debug();
    printf("---\n");
    printf("Parent: \n");
    parent_transform.debug();
    printf("---\n");
    printf("Transform:\n");
    transforms[i].debug();
    printf("---\n");
  }

  shader.use();
  shader.set_int("texture1", 0);
  shader.set_mat4("jointTransforms", &transforms[0].m[0], animation.skeleton.joint_count);

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

    sta_gl_clear_buffer(0.0f, 0.0f, 0.0f, 1.0f);

    glDrawElements(GL_TRIANGLES, animation.vertex_count, GL_UNSIGNED_INT, 0);
    sta_gl_render(window);
  }
}
