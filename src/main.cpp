#include "font.h"
#include "sta_renderer.h"
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

static inline u64 convert_me(i16* coordinates, u64 j, u64 min, u64 max, u64 screen_width)
{
  return (u64)(((coordinates[j] - min) / (f32)(max - min)) * screen_width) / 2 + screen_width / 4;
}

int main()
{

  const int screen_width  = 620;
  const int screen_height = 480;

  Renderer  renderer      = {};
  init_renderer(&renderer, screen_width, screen_height);
  u32        ticks = 0;

  Font font = {};
  sta_font_parse_ttf(&font, "./data/fonts/JetBrainsMono-Bold.ttf");
  // TableGlyf* glyf  = table.glyf;

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
    // SimpleGlyph* glyph = &glyf->simple;
    // u32          prev  = 0;
    // for (u32 i = 0; i < glyph->n; i++)
    // {
    //   u32 end_contour      = glyph->end_pts_of_contours[i];
    //   u32 number_of_points = end_contour - prev + 1;
    //   // printf("Drawing %d points, ending at %d, prev %d\n", number_of_points, end_contour, prev);
    //   for (u32 j = 0; j < number_of_points; j++)
    //   {
    //     // printf("(%d, %d) -> (%d,%d)\n", glyph->x_coordinates[j + prev], glyph->y_coordinates[j + prev], glyph->x_coordinates[(j + 1) % number_of_points + prev],
    //     //        glyph->y_coordinates[(j + 1) % number_of_points + prev]);
    //     u64 start_x = convert_me(glyph->x_coordinates, j + prev, glyf->data.min_x, glyf->data.max_x, screen_width);
    //     u64 end_x   = convert_me(glyph->x_coordinates, (j + 1) % number_of_points + prev, glyf->data.min_x, glyf->data.max_x, screen_width);

    //     u64 start_y = convert_me(glyph->y_coordinates, j + prev, glyf->data.min_y, glyf->data.max_y, screen_height);
    //     u64 end_y   = convert_me(glyph->y_coordinates, (j + 1) % number_of_points + prev, glyf->data.min_y, glyf->data.max_y, screen_height);
    //     // printf("%ld %ld %ld %ld\n", start_x, start_y, end_x, end_y);
    //     draw_line(&renderer.buffer, start_x, start_y, end_x, end_y, RED);
    //   }
    //   prev = end_contour + 1;
    // }

    render(&renderer);
  }
}
