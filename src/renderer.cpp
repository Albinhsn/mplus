#include "renderer.h"
#include "collision.h"
#include "common.h"
#include "font.h"
#include "platform.h"
#include "sdl.h"
#include "vector.h"
#include <GL/gl.h>
#include <GL/glext.h>
void Renderer::toggle_wireframe_on()
{
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}
void Renderer::toggle_wireframe_off()
{
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
void Renderer::change_screen_size(u32 screen_width, u32 screen_height)
{
}

void Renderer::draw_line(f32 x1, f32 y1, f32 x2, f32 y2)
{
  glColor3f(1.0, 1.0, 1.0);
  glBegin(GL_LINES);
  glVertex2f(x1, y1);
  glVertex2f(x2, y2);
  glEnd();
}
void Renderer::get_string_vertices(Vector2** _vertices, u32& vertex_count, const u32 code)
{
  Glyph glyph    = this->font->get_glyph(code);
  u32   contours = glyph.end_pts_of_contours[glyph.n - 1] + 1;
  // get the vertices of the glyph
  Vector2* vertices;
  if (glyph.n == 1)
  {
    vertices     = (Vector2*)sta_allocate_struct(Vector2, contours);
    vertex_count = contours;
    for (u32 j = 0; j < contours; j++)
    {
      Vector2* vertex = &vertices[j];
      vertex->x       = glyph.x_coordinates[j];
      vertex->y       = glyph.y_coordinates[j];
      printf("%d: %f %f\n", j, vertex->x, vertex->y);
    }
    // x += glyph.advance_width * this->font->scale;
    // add x offset
  }
  else
  {
    Vector2** verts        = (Vector2**)sta_allocate_struct(Vector2*, glyph.n);
    u32*      point_counts = (u32*)sta_allocate_struct(u32, glyph.n);
    u32       prev_end     = 0;
    for (u32 i = 0; i < glyph.n; i++)
    {
      u32      end_pts            = glyph.end_pts_of_contours[i];
      u32      number_of_vertices = end_pts - prev_end;
      Vector2* v                  = (Vector2*)sta_allocate_struct(Vector2, number_of_vertices);
      point_counts[i]             = number_of_vertices;
      for (u32 j = 0; j < end_pts - prev_end; j++)
      {
        v[j].x = glyph.x_coordinates[j + prev_end + 1];
        v[j].y = glyph.y_coordinates[j + prev_end + 1];
        printf("%d: (%f, %f)\n", j, v[j].x, v[j].y);
      }
      printf("-\n");
      prev_end = end_pts;
      verts[i] = v;
    }
    triangulation_hole_via_ear_clipping(&vertices, vertex_count, verts, point_counts, glyph.n);
  }
  *_vertices = vertices;
}

void Renderer::render_text(const char* string, u32 string_length, f32 font_size, f32 x, f32 y, TextAlignment alignment_x, TextAlignment alignment_y)
{
  // after precalc is done
  // this should just be querying for the triangles data and rendering
  for (u32 i = 0; i < string_length; i++)
  {
    Glyph glyph    = this->font->get_glyph(string[i]);
    u32   prev_end = 0;
    u32   contours = glyph.end_pts_of_contours[glyph.n - 1] + 1;
    // get the vertices of the glyph
    Vector2* vertices = (Vector2*)sta_allocate_struct(Vector2, contours);
    if (glyph.n == 1)
    {
      for (u32 j = 0; j < contours; j++)
      {
        Vector2* vertex = &vertices[j];
        // u16      glyph_x = glyph.x_coordinates[j];
        // u16      glyph_y = glyph.y_coordinates[j];
        // vertex->x = x + glyph_x * this->font->scale;
        // vertex->y = y + 0.5f * (glyph_y * this->font->scale);
        vertex->x = glyph.x_coordinates[j];
        vertex->y = glyph.y_coordinates[j];
        printf("%d: %f %f\n", j, vertex->x, vertex->y);
      }
      x += glyph.advance_width * this->font->scale;
      // add x offset
    }
    else
    {
      assert(!"Please no :)");
      for (u32 j = 0; j < glyph.n; j++)
      {
        u16 contour = glyph.end_pts_of_contours[j];
        while (prev_end <= contour)
        {

          prev_end++;
        }
      }
    }

    Triangle* triangles;
    u32       triangle_count;

    triangulate_simple_via_ear_clipping(&triangles, triangle_count, vertices, contours);
    // exit(1);
    // for (u32 j = 0; j < triangle_count; j++)
    // {
    //   Triangle t = triangles[j];
    //   t.points[0].debug();
    //   t.points[1].debug();
    //   t.points[2].debug();
    //   printf("-\n");
    // }

    sta_glBindVertexArray(this->text_buffer.vao);
    sta_glBindBuffer(GL_ARRAY_BUFFER, this->text_buffer.vbo);
    sta_glBufferData(GL_ARRAY_BUFFER, sizeof(Triangle) * triangle_count, triangles, GL_DYNAMIC_DRAW);

    u32 index_count = triangle_count * 3;
    u32 triangle_indices[index_count];
    for (u32 j = 0; j < index_count; j++)
    {
      triangle_indices[j] = j;
    }

    sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->text_buffer.ebo);
    sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * index_count, triangle_indices, GL_DYNAMIC_DRAW);
    // exit(1);

    this->text_shader.use();
    Color white = WHITE;
    this->text_shader.set_float4f("color", (float*)&white);

    glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, 0);

    sta_deallocate(vertices, sizeof(Vector2) * glyph.end_pts_of_contours[glyph.n - 1]);
  }
}
void Renderer::bind_texture(u32 texture_id, u32 texture_unit)
{
  glActiveTexture(texture_unit);
  sta_glBindTexture(GL_TEXTURE_2D, texture_id);
}
u32 Renderer::generate_texture(u32 width, u32 height, void* data)
{

  u32 texture;
  sta_glGenTextures(1, &texture);
  sta_glBindTexture(GL_TEXTURE_2D, texture);

  sta_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  sta_glGenerateMipmap(GL_TEXTURE_2D);

  return texture;
}

void Renderer::swap_buffers()
{
  sta_gl_render(this->window);
}
void Renderer::init_quad_buffer_2d()
{
  this->quad_buffer_2d = {};
  sta_glGenVertexArrays(1, &this->quad_buffer_2d.vao);
  sta_glGenBuffers(1, &this->quad_buffer_2d.vbo);
  sta_glGenBuffers(1, &this->quad_buffer_2d.ebo);

  sta_glBindVertexArray(this->quad_buffer_2d.vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, this->quad_buffer_2d.vbo);
  const int vertices_in_a_quad = 8;
  f32       tot[8]             = {
      1.0f,  1.0f,  //
      1.0f,  -1.0,  //
      -1.0f, -1.0f, //
      -1.0f, 1.0,   //
  };
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * vertices_in_a_quad, tot, GL_DYNAMIC_DRAW);

  sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->quad_buffer_2d.ebo);
  u32 indices[6] = {0, 1, 2, 0, 2, 3};
  sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * ArrayCount(indices), indices, GL_STATIC_DRAW);

  sta_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(f32) * 2, (void*)(0));
  sta_glEnableVertexAttribArray(0);

  this->quad_shader = Shader("./shaders/quad.vert", "./shaders/quad.frag");
}
void Renderer::init_text_shader()
{
  this->text_buffer = {};
  sta_glGenVertexArrays(1, &this->text_buffer.vao);
  sta_glGenBuffers(1, &this->text_buffer.vbo);
  sta_glGenBuffers(1, &this->text_buffer.ebo);

  sta_glBindVertexArray(this->text_buffer.vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, this->text_buffer.vbo);
  sta_glBufferData(GL_ARRAY_BUFFER, 0, 0, GL_DYNAMIC_DRAW);

  sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->text_buffer.ebo);
  sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, 0, GL_DYNAMIC_DRAW);

  sta_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(f32) * 2, (void*)(0));
  sta_glEnableVertexAttribArray(0);

  this->text_shader = Shader("./shaders/text.vert", "./shaders/text.frag");
}
void Renderer::render_2d_quad(f32 min[2], f32 max[2], Color color)
{
  sta_glBindVertexArray(this->quad_buffer_2d.vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, this->quad_buffer_2d.vbo);
  f32 tot[8] = {
      max[0], min[1], //
      max[0], max[1], //
      min[0], max[1], //
      min[0], min[1], //
  };
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * 8, tot, GL_DYNAMIC_DRAW);
  this->quad_shader.use();

  color = WHITE;
  this->quad_shader.set_float4f("color", (float*)&color);

  sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->quad_buffer_2d.ebo);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void Renderer::render_arrays(u32 buffer_id, GLenum type, u32 count)
{
  GLBufferIndex buffer = this->index_buffers[buffer_id];
  sta_glBindVertexArray(buffer.vao);

  glDrawArrays(type, 0, count);
}

u32 Renderer::create_buffer(u64 buffer_size, void* buffer_data, BufferAttributes* attributes, u64 attribute_count)
{
  GLBufferIndex buffer = {};
  sta_glGenVertexArrays(1, &buffer.vao);
  sta_glGenBuffers(1, &buffer.vbo);

  sta_glBindVertexArray(buffer.vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, buffer.vbo);
  sta_glBufferData(GL_ARRAY_BUFFER, buffer_size, buffer_data, GL_STATIC_DRAW);

  u32 size = 0;
  for (u32 i = 0; i < attribute_count; i++)
  {
    size += attributes[i].count;
  }
  size *= 4;
  u32 stride = 0;
  for (u32 i = 0; i < attribute_count; i++)
  {

    BufferAttributes attribute = attributes[i];
    switch (attribute.type)
    {
    case GL_FLOAT:
    {
      sta_glVertexAttribPointer(i, attribute.count, GL_FLOAT, GL_FALSE, size, (void*)(stride * sizeof(int)));
      break;
    }
    case GL_INT:
    {
      sta_glVertexAttribIPointer(i, attribute.count, GL_INT, size, (void*)(stride * sizeof(int)));
      break;
    }
    }
    stride += attribute.count;
    sta_glEnableVertexAttribArray(i);
  }

  if (this->index_buffers_cap == 0)
  {
    this->index_buffers_count = 1;
    this->index_buffers_cap   = 1;
    this->index_buffers       = (GLBufferIndex*)sta_allocate_struct(GLBufferIndex, 1);
    this->index_buffers[0]    = buffer;
    return 0;
  }
  RESIZE_ARRAY(this->index_buffers, GLBufferIndex, this->index_buffers_count, this->index_buffers_cap);
  this->index_buffers[this->index_buffers_count] = buffer;
  return this->index_buffers_count++;
}

u32 Renderer::create_buffer_indices(u64 buffer_size, void* buffer_data, u64 index_count, u32* indices, BufferAttributes* attributes, u32 attribute_count)
{
  GLBufferIndex buffer = {};
  buffer.index_count   = index_count;
  sta_glGenVertexArrays(1, &buffer.vao);
  sta_glGenBuffers(1, &buffer.vbo);
  sta_glGenBuffers(1, &buffer.ebo);

  sta_glBindVertexArray(buffer.vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, buffer.vbo);
  sta_glBufferData(GL_ARRAY_BUFFER, buffer_size, buffer_data, GL_STATIC_DRAW);

  sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.ebo);
  sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * index_count, indices, GL_STATIC_DRAW);

  u32 size = 0;
  for (u32 i = 0; i < attribute_count; i++)
  {
    size += attributes[i].count;
  }
  size *= 4;
  u32 stride = 0;
  for (u32 i = 0; i < attribute_count; i++)
  {

    BufferAttributes attribute = attributes[i];
    switch (attribute.type)
    {
    case GL_FLOAT:
    {
      sta_glVertexAttribPointer(i, attribute.count, GL_FLOAT, GL_FALSE, size, (void*)(stride * sizeof(int)));
      break;
    }
    case GL_INT:
    {
      sta_glVertexAttribIPointer(i, attribute.count, GL_INT, size, (void*)(stride * sizeof(int)));
      break;
    }
    }
    stride += attribute.count;
    sta_glEnableVertexAttribArray(i);
  }

  if (this->index_buffers_cap == 0)
  {
    this->index_buffers_count = 1;
    this->index_buffers_cap   = 1;
    this->index_buffers       = (GLBufferIndex*)sta_allocate_struct(GLBufferIndex, 1);
    this->index_buffers[0]    = buffer;
    return 0;
  }
  RESIZE_ARRAY(this->index_buffers, GLBufferIndex, this->index_buffers_count, this->index_buffers_cap);
  this->index_buffers[this->index_buffers_count] = buffer;
  return this->index_buffers_count++;
}
void Renderer::render_buffer(u32 buffer_id)
{
  sta_glBindVertexArray(this->index_buffers[buffer_id].vao);
  glDrawElements(GL_TRIANGLES, this->index_buffers[buffer_id].index_count, GL_UNSIGNED_INT, 0);
}

void Renderer::clear_framebuffer()
{
  sta_gl_clear_buffer(0, 0, 0, 1.0f);
}
