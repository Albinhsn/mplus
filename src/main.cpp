#include "common.h"
#include "font.h"
#include "input.h"
#include "platform.h"
#include "sdl.h"
#include "vector.h"
#include <cassert>
#include <cfloat>
#include <cstdlib>

struct Triangle
{
  Vector2 points[3];
};

inline float orient_2d(Vector2 v0, Vector2 v1, Vector2 p)
{
  return (v0.x - p.x) * (v1.y - p.y) - (v0.y - p.y) * (v1.x - p.x);
}

// assumes that it is a simple polygon!
void triangulate_simple_via_ear_clipping(Triangle** out, Vector2* v_points, u32 point_count)
{
  struct PointDLL
  {
    PointDLL* prev;
    PointDLL* next;
    Vector2   point;
    u32       idx;
  };
  PointDLL* points = (PointDLL*)sta_allocate_struct(PointDLL, point_count);

  // constructs a doubly linked list of the points
  for (i32 i = 0; i < point_count; i++)
  {
    points[i].point = v_points[i];
    points[i].next  = &points[(i + 1) % point_count];
    points[i].prev  = &points[i - 1 < 0 ? point_count - 1 : i - 1];
    points[i].idx   = i;
  }

  u32 ears[point_count];
  u32 ear_count = 0;
  // find the ears
  for (u32 i = 0; i < point_count; i++)
  {
    // get the triangle made up of the 3 vertices
    PointDLL* p1   = &points[i];
    PointDLL* p0   = p1->prev;
    PointDLL* p2   = p1->next;

    PointDLL* curr = p2->next;

    // wierd if it's only three points
    bool found_contained_point = false;
    while (curr != p0)
    {
      float d0, d1, d2;
      bool  neg, pos;

      d0 = orient_2d(p0->point, p1->point, curr->point);
      d1 = orient_2d(p1->point, p2->point, curr->point);
      d2 = orient_2d(p2->point, p0->point, curr->point);

      // check both ways since we don't know the order :)
      neg = (d0 < 0) && (d1 < 0) && (d2 < 0);
      pos = (d0 > 0) && (d1 > 0) && (d2 > 0);
      if (neg || pos)
      {
        found_contained_point = true;
        break;
      }

      curr = curr->next;
    }
    if (!found_contained_point)
    {
      ears[ear_count++] = i;
    }
  }

  for (u32 i = 0; i < ear_count; i++)
  {
    printf("Ear: ");
    points[ears[i]].point.debug();
  }
}

struct TextData
{
  Vector2*  points;
  Triangle* on_curve_points;
  u32*      contours;
  u32       on_curve_point_count;
  u32       contour_count;
  u32       point_count;
};

void get_text_data(Font* font, TextData* text_data, u32* text, u32 text_count)
{
  Vector2* on_curve_points;
  u32      on_curve_point_count;
  for (u32 i = 0; i < text_count; i++)
  {
    Glyph glyph = font->get_glyph(text[i]);
  }

  triangulate_simple_via_ear_clipping(&text_data->on_curve_points, on_curve_points, on_curve_point_count);
}

int main()
{

  const int     screen_width  = 620;
  const int     screen_height = 480;

  InputState    input_state   = {};

  SDL_GLContext context;
  SDL_Window*   window;
  sta_init_sdl_gl(&window, &context, screen_width, screen_height);
  u32  ticks = 0;

  Font font  = {};
  font.parse_ttf("./data/fonts/JetBrainsMono-Bold.ttf");

  char upper_chars[] = "ABCDEFGHIJKLMNOPQRSTUVXYZ";
  char lower_chars[] = "abcdefghijklmnopqrstuvwyz";


  while (true)
  {

    input_state.update();
    if (input_state.should_quit())
    {
      break;
    }

    if (ticks + 16 < SDL_GetTicks())
    {
      ticks = SDL_GetTicks() + 16;
    }

    sta_gl_clear_buffer(1.0, 1.0, 1.0, 1.0);

    sta_gl_render(window);
  }
}
