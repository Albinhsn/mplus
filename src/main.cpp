#include "font.h"
#include "sta_renderer.h"
#include "vector.h"
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

static inline f32 convert_me(u64 x, u64 min, u64 max)
{
  if (min == max)
  {
    return 0;
  }
  return ((x - min) / (f32)(max - min));
}

static void render_glyph(Glyph glyph, FrameBuffer* buffer, const int width, const int height, const int offset_x, const int offset_y)
{
  u32 prev = 0;
  for (u32 i = 0; i < glyph.n; i++)
  {
    u32 end_contour      = glyph.end_pts_of_contours[i];
    u32 number_of_points = end_contour - prev;
    for (u32 point_index = 0; point_index <= number_of_points; point_index++)
    {
      u64 p_index = prev + point_index;
      f32 start_x = convert_me(glyph.x_coordinates[p_index], glyph.min_x, glyph.max_x);
      f32 start_y = 1.0f - convert_me(glyph.y_coordinates[p_index], glyph.min_y, glyph.max_y);
      f32 end_x   = convert_me(glyph.x_coordinates[(point_index + 1) % (number_of_points + 1) + prev], glyph.min_x, glyph.max_x);
      f32 end_y   = 1.0f - convert_me(glyph.y_coordinates[(point_index + 1) % (number_of_points + 1) + prev], glyph.min_y, glyph.max_y);

      u64 sx      = offset_x + width * start_x;
      u64 sy      = offset_y + height * start_y;
      u64 ex      = offset_x + width * end_x;
      u64 ey      = offset_y + height * end_y;
      draw_line(buffer, sx, sy, ex, ey, GREEN);
    }

    prev = end_contour + 1;
  }
}

int main()
{

  const int screen_width  = 620;
  const int screen_height = 480;

  Renderer  renderer      = {};
  init_renderer(&renderer, screen_width, screen_height);
  u32  ticks = 0;

  Font font  = {};
  sta_font_parse_ttf(&font, "./data/fonts/JetBrainsMono-Bold.ttf");

  const int quadrants       = 100;
  const int quad_per_row    = sqrt(quadrants);
  const int quadrant_width  = screen_width / quad_per_row - 10;
  const int quadrant_height = screen_height / quad_per_row - 10;

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
    for (u32 row = 0; row < quad_per_row; row++)
    {
      for (u32 col = 0; col < quad_per_row; col++)
      {
        u32 glyph_index = row * quad_per_row + col;
        assert(glyph_index < font.glyph_count && "Really?");

        u64   offset_x = col * quadrant_width;
        u64   offset_y = row * quadrant_height;
        Glyph glyph    = font.glyphs[glyph_index];
        render_glyph(glyph, &renderer.buffer, quadrant_width, quadrant_height, offset_x, offset_y);
      }
    }
    // figure out lines to draw between 0 and 1

    render(&renderer);
  }
}
