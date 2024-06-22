#include "common.h"
#include "font.h"
#include "input.h"
#include "collision.h"
#include "platform.h"
#include "renderer.h"
#include "shader.h"
#include "ui.h"
#include "vector.h"


int main()
{

  Font font;
  font.parse_ttf("./data/fonts/JetBrainsMono-Bold.ttf");
  const int  screen_width  = 620;
  const int  screen_height = 480;
  Renderer   renderer(screen_width, screen_height, 0, 0);
  renderer.font = &font;

  f32        min[2]      = {-1.0f, -1.0f};
  f32        max[2]      = {1.0f, 1.0f};

  InputState input_state = {};
  u32        ticks       = 0;
  UI         ui(&renderer, &input_state);

  Vector2    points[10];
  points[0]              = Vector2(-0.6f, -0.2f);
  points[1]              = Vector2(-0.3f, -0.4f);
  points[2]              = Vector2(0.0f, -0.16f);
  points[3]              = Vector2(0.3f, -0.3f);
  points[4]              = Vector2(0.5f, 0.0f);
  points[5]              = Vector2(0.2f, -0.05f);
  points[6]              = Vector2(0.0f, 0.3f);
  points[7]              = Vector2(-0.2f, -0.24f);
  points[8]              = Vector2(-0.48f, -0.13f);
  points[9]              = Vector2(-0.46f, 0.15f);

  Vector2 point_data[20] = {
      points[0], points[1], points[1], points[2], points[2], points[3], points[3], points[4], points[4], points[5],
      points[5], points[6], points[6], points[7], points[7], points[8], points[8], points[9], points[9], points[0],
  };
  for (u32 i = 0; i < ArrayCount(point_data); i++)
  {
    point_data[i].x = (point_data[i].x - 1.0f) / 2.0f;
  }

  Triangle* result;
  u32       result_count;
  triangulate_simple_via_ear_clipping(&result, result_count, points, ArrayCount(points));
  u32  indices_count  = result_count * 3;
  u32* result_indices = (u32*)sta_allocate_struct(u32, indices_count);
  for (u32 i = 0; i < indices_count; i++)
  {
    result_indices[i] = i;
  }
  for (u32 i = 0; i < result_count; i++)
  {
    result[i].points[0].x = (result[i].points[0].x + 1.0f) / 2.0f;
    result[i].points[1].x = (result[i].points[1].x + 1.0f) / 2.0f;
    result[i].points[2].x = (result[i].points[2].x + 1.0f) / 2.0f;
  }

  Shader line_shader("./shaders/line.vert", "./shaders/line.frag");
  Shader tri_shader("./shaders/tri.vert", "./shaders/tri.frag");

  renderer.toggle_wireframe_on();

  // draw lines for each triangle
  // run algorithm and render triangles
  // do this split screen style

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

    renderer.render_text("l", 1, 0.1f, 0, 0, (TextAlignment)0, (TextAlignment)0);

    renderer.swap_buffers();
  }
}
