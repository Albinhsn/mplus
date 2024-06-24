#include "common.h"
#include "font.h"
#include "input.h"
#include "renderer.h"
#include "ui.h"

void run_debug(Font font, Renderer renderer, u32 ticks, InputState input_state)
{

  Triangle*        triangles;
  u32              triangle_count;

  EarClippingNodes tri = {};

  Vector2*         v_points;
  u32              point_count;

  f32              x = 0;
  Glyph            g = font.get_glyph('0');
  renderer.triangulate_simple_glyph(&v_points, point_count, x, x, &g, 0.15f);

  get_vertices(&tri, v_points, point_count);
  debug_points(&tri);

  triangle_count = point_count - 2;
  triangles      = (Triangle*)sta_allocate_struct(Triangle, triangle_count);
  u32 remaining  = point_count;
  triangle_count = 0;

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

    if (input_state.is_key_released('q'))
    {
      if (remaining >= 3)
      {
        remove_vertex(&triangles[triangle_count++], &tri);
        if (remaining == 3)
        {
          triangle_count++;
          remaining--;
        }
      }
    }
    renderer.clear_framebuffer();

    // draw vertices on left half
    for (u32 i = 0; i < point_count; i++)
    {
      i32     high = i + 1 >= point_count ? 0 : i + 1;
      Vector2 p0   = tri.head[i].point;
      Vector2 p1   = tri.head[high].point;
      p0.x         = p0.x * renderer.font->scale - 1.0f;
      p0.y         = 0.5f * (p0.y * renderer.font->scale);
      p1.x         = p1.x * renderer.font->scale - 1.0f;
      p1.y         = 0.5f * (p1.y * renderer.font->scale);
      renderer.draw_line(p0.x, p0.y, p1.x, p1.y);
    }
    // draw current triangles on right
    for (u32 i = 0; i < triangle_count; i++)
    {
      Triangle triangle = triangles[i];
      Vector2  p0       = triangle.points[0];
      p0.x              = p0.x - 0.25f;
      Vector2 p1        = triangle.points[1];
      p1.x              = p1.x - 0.25f;
      Vector2 p2        = triangle.points[2];
      p2.x              = p2.x - 0.25f;
      renderer.draw_line(p0.x, p0.y, p1.x, p1.y);
      renderer.draw_line(p1.x, p1.y, p2.x, p2.y);
      renderer.draw_line(p2.x, p2.y, p0.x, p0.y);
    }
    if (remaining >= 3)
    {
      EarClippingNode* ear = tri.ear;
      for (u32 i = 0; i <= remaining; i++)
      {
        Vector2 p0 = ear->point;
        p0.x       = p0.x + 0.25f;
        Vector2 p1 = ear->next->point;
        p1.x       = p1.x + 0.25f;
        renderer.draw_line(p0.x, p0.y, p1.x, p1.y);
        ear = ear->next;
      }
    }

    renderer.swap_buffers();
  }
}

int main()
{

  Font font;
  font.parse_ttf("./data/fonts/OpenSans-Regular.ttf");
  const int  screen_width  = 620;
  const int  screen_height = 480;
  Renderer   renderer(screen_width, screen_height, &font, 0);

  f32        min[2]      = {-1.0f, -1.0f};
  f32        max[2]      = {1.0f, 1.0f};

  InputState input_state = {};
  u32        ticks       = 0;
  UI         ui(&renderer, &input_state);

  // run_debug(font, renderer, ticks, input_state);
  // return 0;

  ui.add_layout(min, max);
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

    renderer.clear_framebuffer();

    // font size, wrap
    const char* s = "abcdefghklmnopqrstuvxyz";
    renderer.render_text(s, strlen(s), -0.85f, 0.0f, (TextAlignment)0, (TextAlignment)0, WHITE, 0.1f);

    const char* s2 = "ABCDEFGHIJKLMNOPQRSTUVXYZ";
    renderer.render_text(s2, strlen(s2), -0.85f, 0.15f, (TextAlignment)0, (TextAlignment)0, WHITE, 0.1f);

    const char* s3 = "0123456789";
    renderer.render_text(s3, strlen(s3), -0.85f, -0.15f, (TextAlignment)0, (TextAlignment)0, WHITE, 0.1f);

    renderer.swap_buffers();
  }
}
