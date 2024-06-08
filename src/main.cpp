#include "animation.h"
#include "files.h"
#include "sdl.h"
#include "shader.h"
#include <GL/glext.h>
#include <SDL2/SDL_events.h>
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
bool debug_me(ModelData* obj, AnimationModel* collada)
{
  const float EPSILON = 0.0001f;
  for (u64 i = 0; i < obj->vertex_count; i++)
  {
    u64 obj_index     = obj->indices[i];
    u64 collada_index = collada->indices[i];
    if (obj_index != collada_index)
    {
      printf("Wrong index @%ld, %ld vs %ld\n", i, obj_index, collada_index);
    }
    VertexData    obj_vertex     = obj->vertices[i];
    SkinnedVertex collada_vertex = collada->vertices[i];
    if (std::abs(obj_vertex.vertex.x - collada_vertex.position.x) > EPSILON)
    {
      printf("Failed x @%ld, %f vs %f\n", i, obj_vertex.vertex.x, collada_vertex.position.x);
      // return false;
    }
    if (std::abs(obj_vertex.vertex.y - collada_vertex.position.y) > EPSILON)
    {
      printf("Failed y @%ld, %f vs %f\n", i, obj_vertex.vertex.y, collada_vertex.position.y);
      // return false;
    }
    if (std::abs(obj_vertex.vertex.z - collada_vertex.position.z) > EPSILON)
    {
      printf("Failed normal x @%ld, %f vs %f\n", i, obj_vertex.vertex.x, collada_vertex.position.x);
      return false;
    }
  }
  return true;
}

int main()
{

  AnimationModel animation = {};
  sta_collada_parse_from_file(&animation, "./data/unarmed_man/unarmed_opt.dae");

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

  result = debug_me(&model, &animation);
  if (!result)
  {
    printf("Failed\n");
  }
  return 0;

  TargaImage image = {};
  result           = sta_read_targa_from_file(&image, "./data/unarmed_man/Peasant_Man_diffuse.tga");
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

  int size = sizeof(float) * 12;
  sta_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, size, (void*)0);
  sta_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, size, (void*)(3 * sizeof(float)));
  sta_glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, size, (void*)(5 * sizeof(float)));
  sta_glVertexAttribIPointer(3, 1, GL_INT, size, (void*)(8 * sizeof(float)));
  sta_glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, size, (void*)(9 * sizeof(float)));
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

  Shader shader("./shaders/tri.vert", "./shaders/tri.frag");

  shader.use();
  shader.set_int("texture1", 0);

  glActiveTexture(GL_TEXTURE0);
  sta_glBindTexture(GL_TEXTURE_2D, texture0);
  sta_glBindVertexArray(vao);

  u32   ticks = 0;

  Mat44 view  = {};
  view.identity();

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
