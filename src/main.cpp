#include "common.h"
#include "font.h"
#include "input.h"
#include "platform.h"
#include "renderer.h"
#include "shader.h"
#include "ui.h"
#include "vector.h"

struct Triangle
{
public:
  Triangle()
  {
  }
  Triangle(Vector2 v0, Vector2 v1, Vector2 v2)
  {
    points[0] = v0;
    points[1] = v1;
    points[2] = v2;
  }
  Vector2 points[3];
};

inline float orient_2d(Vector2 v0, Vector2 v1, Vector2 p)
{
  return (v0.x - p.x) * (v1.y - p.y) - (v0.y - p.y) * (v1.x - p.x);
}
struct PointDLL
{
  PointDLL* prev;
  PointDLL* next;
  Vector2   point;
  u32       idx;
};
void get_reflex_vertices(PointDLL** _reflex, u32& reflex_count, Vector2* v_points, u32 point_count)
{
}
void get_convex_vertices(PointDLL** _convex, u32& convex_count, Vector2* v_points, u32 point_count)
{
}

void get_ear_vertices(PointDLL** _ears, u32& ear_count, Vector2* v_points, u32 point_count)
{
  PointDLL* ears = (PointDLL*)sta_allocate_struct(PointDLL, point_count);

  // constructs a doubly linked list of the points
  for (i32 i = 0; i < point_count; i++)
  {
    ears[i].point = v_points[i];
    ears[i].next  = &ears[(i + 1) % point_count];
    ears[i].prev  = &ears[i - 1 < 0 ? point_count - 1 : i - 1];
    ears[i].idx   = i;
  }

  *_ears = ears;
}

// assumes that it is a simple polygon!
void triangulate_simple_via_ear_clipping(Triangle** out, u32& out_count, Vector2* v_points, u32 point_count)
{
  // find ears
  PointDLL* ears;
  u32       ear_count;
  get_ear_vertices(&ears, ear_count, v_points, point_count);

  // find convex vertices
  PointDLL* convex;
  u32       convex_count;
  get_convex_vertices(&convex, convex_count, v_points, point_count);

  // find reflex vertices
  PointDLL* reflex;
  u32       reflex_count;
  get_reflex_vertices(&reflex, reflex_count, v_points, point_count);

  // remove one ear at a time
  //
  // if an adjacent vertex is removed
  //  still convex
  //  maybe not an ear
  //  reflex might be an ear or convex
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

  // check the ordering of vertices, if all are in the same order then simple, otherwise hole(s)

  triangulate_simple_via_ear_clipping(&text_data->on_curve_points, text_data->on_curve_point_count, on_curve_points, on_curve_point_count);
}

int main()
{

  const int  screen_width  = 620;
  const int  screen_height = 480;
  Renderer   renderer(screen_width, screen_height, 0, 0);

  f32        min[2]      = {-1.0f, -1.0f};
  f32        max[2]      = {1.0f, 1.0f};

  InputState input_state = {};
  u32        ticks       = 0;
  UI         ui(&renderer, &input_state);

  Vector2    points[10];
  points[0]              = Vector2(-0.6f, -0.2f);
  points[1]              = Vector2(-0.3f, -0.4f);
  points[2]              = Vector2(0.0f, -0.2f);
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
  u32*      result_indices = (u32*)sta_allocate_struct(u32, result_count);
  triangulate_simple_via_ear_clipping(&result, result_count, points, ArrayCount(points));
  for (u32 i = 0; i < result_count; i++)
  {
    result_indices[i] = i;
  }

  Shader           line_shader("./shaders/line.vert", "./shaders/line.frag");
  Shader           tri_shader("./shaders/tri.vert", "./shaders/tri.frag");

  BufferAttributes attributes[1]     = {2, GL_FLOAT};
  BufferAttributes tri_attributes[1] = {3, GL_FLOAT};

  u32              line_buffer       = renderer.create_buffer(sizeof(Vector2) * ArrayCount(point_data), (void*)point_data, attributes, ArrayCount(attributes));
  u32              tri_buffer = renderer.create_buffer_indices(sizeof(Triangle) * result_count, (void*)result, result_count * sizeof(u32), result_indices, tri_attributes, ArrayCount(tri_attributes));

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

    // line_shader.use();
    // renderer.render_arrays(line_buffer, GL_LINES, ArrayCount(point_data));

    // tri_shader.use();
    // renderer.render_buffer(tri_buffer);

    String s = {};
    f32 loc_min[2] = {-0.25f, 0.25f};
    f32 loc_max[2] = {0.25f, -0.25f};
    if (ui.UI_Button(s, loc_min, loc_max).left_clicked)
    {
      printf("Clicked!\n");
    }

    renderer.swap_buffers();
  }
}
