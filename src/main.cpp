#include "common.h"
#include "font.h"
#include "sdl.h"
#include "shader.h"
#include "sta_renderer.h"
#include "vector.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL_events.h>
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

static void render_glyph(Font* font, Glyph glyph, FrameBuffer* buffer, const int width, const int height, const int offset_x, const int offset_y, f32 scale)
{
  u32 prev = 0;
  for (u32 i = 0; i < glyph.n; i++)
  {
    // idea
    // don't use min/max but rather the scale?
    u32 end_contour      = glyph.end_pts_of_contours[i];
    u32 number_of_points = end_contour - prev;
    for (u32 point_index = 0; point_index <= number_of_points; point_index++)
    {
      u64 p_index = prev + point_index;
      f32 start_x = glyph.x_coordinates[p_index] * scale;
      f32 start_y = 1.0f - glyph.y_coordinates[p_index] * scale;
      f32 end_x   = glyph.x_coordinates[(point_index + 1) % (number_of_points + 1) + prev] * scale;
      f32 end_y   = 1.0f - glyph.y_coordinates[(point_index + 1) % (number_of_points + 1) + prev] * scale;

      u64 sx      = offset_x + width * start_x;
      u64 sy      = offset_y + height * start_y;
      u64 ex      = offset_x + width * end_x;
      u64 ey      = offset_y + height * end_y;
      draw_line(buffer, sx, sy, ex, ey, GREEN);
    }

    prev = end_contour + 1;
  }
}

Glyph get_glyph(Font* font, u32 code)
{
  for (u32 i = 0; i < font->glyph_count; i++)
  {
    if (font->char_codes[i] == code)
    {
      return font->glyphs[font->glyph_indices[i]];
    }
  }
  return font->glyphs[0];
}

int main()
{

  const int     screen_width  = 620;
  const int     screen_height = 480;

  SDL_GLContext context;
  SDL_Window*   window;
  sta_init_sdl_gl(&window, &context, screen_width, screen_height);
  u32  ticks = 0;

  Font font  = {};
  sta_font_parse_ttf(&font, "./data/fonts/JetBrainsMono-Bold.ttf");

  Shader shader("./shaders/text.vert", "./shaders/text.tcs", "./shaders/text.tes", "./shaders/text.frag");

  GLuint vao, vbo;
  sta_glCreateVertexArrays(1, &vao);
  sta_glCreateBuffers(1, &vbo);
  sta_glVertexArrayVertexBuffer(vao, 0, vbo, 0, 3 * sizeof(f32));
  sta_glNamedBufferStorage(vbo, 3 * 3 * 4, 0, GL_DYNAMIC_STORAGE_BIT);

  sta_glEnableVertexArrayAttrib(vao, 0);
  sta_glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
  sta_glVertexArrayAttribBinding(vao, 0, 0);

  float vertices[]{
      0,    -0.5, 0, //
      -0.5, 0,    0, //
      0.5,  0,    0  //
  };
  sta_glNamedBufferSubData(vbo, 0, sizeof(float) * 9, vertices);

  char upper_chars[] = "ABCDEFGHIJKLMNOPQRSTUVXYZ";
  char lower_chars[] = "abcdefghijklmnopqrstuvwyz";

  shader.use();
  shader.set_float("segmentCount", 40);
  shader.set_float("stripCount", 1);
  Color color(1.0, 1.0, 0, 1.0);
  shader.set_float4f("inColor", (float*)&color);

  // contours
  // points
  while (true)
  {

    if (should_quit())
    {
      break;
    }
    if (ticks + 16 < SDL_GetTicks())
    {
      ticks = SDL_GetTicks() + 16;
    }

    sta_gl_clear_buffer(1.0, 1.0, 1.0, 1.0);

    sta_glBindVertexArray(vao);
    glDrawArrays(GL_PATCHES, 0, 3);

    sta_gl_render(window);
  }
}
