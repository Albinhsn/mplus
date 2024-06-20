#include "animation.h"
#include "common.h"
#include "files.h"
#include "font.h"
#include "gltf.h"
#include "input.h"
#include "platform.h"
#include "renderer.h"
#include "shader.h"
#include "vector.h"

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

  const int      screen_width  = 620;
  const int      screen_height = 480;
  AnimationModel model         = {};
  Renderer       renderer(screen_width, screen_height);
  if (!parse_gltf(&model, "./data/unarmed_still.glb"))
  {
    printf("Failed to parse model!\n");
    return 1;
  }

  InputState input_state = {};

  u32        ticks       = 0;

  Shader     shader("./shaders/animation.vert", "./shaders/animation.frag");

  TargaImage image = {};
  sta_read_targa_from_file_rgba(&image, "./data/unarmed_man/Peasant_Man_diffuse.tga");

  BufferAttributes attributes[5] = {
      {3, GL_FLOAT},
      {2, GL_FLOAT},
      {3, GL_FLOAT},
      {4, GL_FLOAT},
      {4,   GL_INT}
  };
  u32   buffer_id = renderer.create_buffer(sizeof(SkinnedVertex) * model.vertex_count, model.vertices, model.index_count, model.indices, attributes, ArrayCount(attributes));

  Mat44 view      = {};
  view.identity();
  view = view.scale(0.005f);
  view = view.rotate_y(90.0f);
  view = view.rotate_z(90.0f);
  view = view.translate(Vector3(-0.2f, -0.2f, 0));

  shader.use();
  shader.set_int("texture1", 0);

  u32 texture = renderer.generate_texture(image.width, image.height, image.data);
  renderer.bind_texture(texture, GL_TEXTURE0);

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
      // view  = view.rotate_y(1.0f);
      // view  = view.rotate_x(1.0f);
      shader.set_mat4("view", view);
    }
    update_animation(model, shader, ticks);

    renderer.clear_framebuffer();
    renderer.render_buffer(buffer_id);

    renderer.draw();
  }
}
