#include "animation.h"
#include "files.h"
#include "sdl.h"
#include "shader.h"
#include <GL/glext.h>
#include <SDL2/SDL_events.h>

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
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(VertexData) * model.vertex_count, model.vertices, GL_STATIC_DRAW);

  sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * model.vertex_count, model.indices, GL_STATIC_DRAW);

  sta_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
  sta_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
  sta_glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
  sta_glEnableVertexAttribArray(0);
  sta_glEnableVertexAttribArray(1);
  sta_glEnableVertexAttribArray(2);

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

    glDrawElements(GL_TRIANGLES, model.vertex_count, GL_UNSIGNED_INT, 0);
    sta_gl_render(window);
  }
}
