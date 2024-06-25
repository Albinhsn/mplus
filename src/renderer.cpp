#include "renderer.h"
#include "collision.h"
#include "common.h"
#include "font.h"
#include "platform.h"
#include "sdl.h"
#include "vector.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <cfloat>
void Renderer::enable_2d_rendering()
{
  glDisable(GL_DEPTH_TEST);
}
void Renderer::disable_2d_rendering()
{

  glEnable(GL_DEPTH_TEST);
}

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

static void triangulate_simple_glyph(Vector2** _vertices, u32& vertex_count, i16& x_offset, i16& y_offset, Glyph* glyph)
{
  SimpleGlyph* simple   = &glyph->simple;
  u32          contours = simple->end_pts_of_contours[simple->n - 1] + 1;
  // get the vertices of the glyph
  Vector2* vertices;
  if (simple->n == 1)
  {
    vertices     = (Vector2*)sta_allocate_struct(Vector2, contours);
    vertex_count = contours;
    for (u32 j = 0; j < contours; j++)
    {
      Vector2* vertex = &vertices[j];
      vertex->x       = simple->x_coordinates[j] + x_offset;
      vertex->y       = simple->y_coordinates[j] + y_offset;
    }
  }
  else
  {
    Vector2** verts        = (Vector2**)sta_allocate_struct(Vector2*, simple->n);
    u32*      point_counts = (u32*)sta_allocate_struct(u32, simple->n);
    u32       prev_end     = 0;
    for (u32 i = 0; i < simple->n; i++)
    {
      u32      end_pts            = simple->end_pts_of_contours[i];
      u32      number_of_vertices = end_pts - prev_end;
      Vector2* v                  = (Vector2*)sta_allocate_struct(Vector2, number_of_vertices);
      point_counts[i]             = number_of_vertices + 1;
      for (u32 j = 0; j <= end_pts - prev_end; j++)
      {
        u32 idx = j + prev_end;
        v[j].x  = simple->x_coordinates[idx] + x_offset;
        v[j].y  = simple->y_coordinates[idx] + y_offset;
      }
      prev_end = end_pts + 1;
      verts[i] = v;
    }
    create_simple_polygon_from_polygon_with_holes(&vertices, vertex_count, verts, point_counts, simple->n);
  }
  x_offset += glyph->advance_width;
  *_vertices = vertices;
}

static void triangulate_compound_glyph(Vector2*** v_points, u32** point_count, u32& count, u32& v_cap, u32& p_cap, i16& x, i16& y, Glyph* glyph)
{
  i16 x_offset = x, y_offset = y;
  for (u32 i = 0; i < glyph->compound.glyph_count; i++)
  {
    Glyph* g = &glyph->compound.glyphs[i];
    if (g->s)
    {
      RESIZE_ARRAY((*v_points), Vector2*, count, v_cap);
      RESIZE_ARRAY((*point_count), u32, count, p_cap);
      triangulate_simple_glyph(&(*v_points)[count], (*point_count)[count], x_offset, y_offset, g);
      count++;
    }
    else
    {
      triangulate_compound_glyph(v_points, point_count, count, v_cap, p_cap, x_offset, y_offset, g);
    }
    x_offset = x, y_offset = y;
  }
  x += glyph->advance_width;
}

static void align_text(Triangle* triangles, u32 triangle_count, TextAlignment alignment_x, TextAlignment alignment_y, f32 s, f32 x, f32 y)
{
  switch (alignment_x)
  {
  case TextAlignment_End_At:
  {
    f32 max = -FLT_MAX;
    for (u32 i = 0; i < triangle_count; i++)
    {
      Triangle* tri = &triangles[i];
      max           = MAX(max, tri->points[0].x);
      max           = MAX(max, tri->points[1].x);
      max           = MAX(max, tri->points[2].x);
    }
    for (u32 i = 0; i < triangle_count; i++)
    {
      Triangle* tri    = &triangles[i];
      tri->points[0].x = (tri->points[0].x - max) * s + x;
      tri->points[1].x = (tri->points[1].x - max) * s + x;
      tri->points[2].x = (tri->points[2].x - max) * s + x;
    }
    break;
  }
  case TextAlignment_Centered:
  {
    f32 min = FLT_MAX, max = -FLT_MAX;
    for (u32 i = 0; i < triangle_count; i++)
    {
      Triangle* tri = &triangles[i];
      min           = MIN(min, tri->points[0].x);
      min           = MIN(min, tri->points[1].x);
      min           = MIN(min, tri->points[2].x);

      max           = MAX(max, tri->points[0].x);
      max           = MAX(max, tri->points[1].x);
      max           = MAX(max, tri->points[2].x);
    }
    f32 half = (max - min) / 2.0f;
    for (u32 i = 0; i < triangle_count; i++)
    {
      Triangle* tri    = &triangles[i];
      tri->points[0].x = (tri->points[0].x - half) * s + x;
      tri->points[1].x = (tri->points[1].x - half) * s + x;
      tri->points[2].x = (tri->points[2].x - half) * s + x;
    }
    break;
  }
  case TextAlignment_Start_At:
  {
    for (u32 i = 0; i < triangle_count; i++)
    {
      Triangle* tri    = &triangles[i];
      tri->points[0].x = tri->points[0].x * s + x;
      tri->points[1].x = tri->points[1].x * s + x;
      tri->points[2].x = tri->points[2].x * s + x;
    }
    break;
  }
  }

  switch (alignment_y)
  {
  case TextAlignment_End_At:
  {
    f32 max = -FLT_MAX;
    for (u32 i = 0; i < triangle_count; i++)
    {
      Triangle* tri = &triangles[i];

      max           = MAX(max, tri->points[0].y);
      max           = MAX(max, tri->points[1].y);
      max           = MAX(max, tri->points[2].y);
    }
    for (u32 i = 0; i < triangle_count; i++)
    {
      Triangle* tri    = &triangles[i];
      tri->points[0].y = (tri->points[0].y - max) * s + y;
      tri->points[1].y = (tri->points[1].y - max) * s + y;
      tri->points[2].y = (tri->points[2].y - max) * s + y;
    }
    break;
  }
  case TextAlignment_Centered:
  {
    f32 min = FLT_MAX, max = -FLT_MAX;
    for (u32 i = 0; i < triangle_count; i++)
    {
      Triangle* tri = &triangles[i];
      min           = MIN(min, tri->points[0].y);
      min           = MIN(min, tri->points[1].y);
      min           = MIN(min, tri->points[2].y);

      max           = MAX(max, tri->points[0].y);
      max           = MAX(max, tri->points[1].y);
      max           = MAX(max, tri->points[2].y);
    }
    f32 half = (max - min) / 2.0f;
    for (u32 i = 0; i < triangle_count; i++)
    {
      Triangle* tri    = &triangles[i];
      tri->points[0].y = (tri->points[0].y - half) * s + y;
      tri->points[1].y = (tri->points[1].y - half) * s + y;
      tri->points[2].y = (tri->points[2].y - half) * s + y;
    }
    break;
  }
  case TextAlignment_Start_At:
  {
    for (u32 i = 0; i < triangle_count; i++)
    {
      Triangle* tri    = &triangles[i];
      tri->points[0].y = tri->points[0].y * s + y;
      tri->points[1].y = tri->points[1].y * s + y;
      tri->points[2].y = tri->points[2].y * s + y;
    }
    break;
  }
  }
}

void Renderer::render_text(const char* string, u32 string_length, f32 x, f32 y, TextAlignment alignment_x, TextAlignment alignment_y, Color color, f32 font_size)
{

  u32       count       = 0;
  u32       v_cap       = 2;
  u32       p_cap       = 2;
  Vector2** v_points    = (Vector2**)sta_allocate_struct(Vector2*, v_cap);
  u32*      point_count = (u32*)sta_allocate_struct(u32, p_cap);

  i16       x_offset = 0, y_offset = 0;
  for (u32 i = 0; i < string_length; i++)
  {
    Glyph glyph = this->font->get_glyph(string[i]);
    if (glyph.s)
    {
      RESIZE_ARRAY(v_points, Vector2*, count, v_cap);
      RESIZE_ARRAY(point_count, u32, count, p_cap);

      triangulate_simple_glyph(&v_points[count], point_count[count], x_offset, y_offset, &glyph);
      count++;
    }
    else
    {
      triangulate_compound_glyph(&v_points, &point_count, count, v_cap, p_cap, x_offset, y_offset, &glyph);
    }
  }

  u32 number_of_triangles = 0;
  for (u32 i = 0; i < count; i++)
  {
    number_of_triangles += point_count[i] - 2;
  }

  Triangle* triangles      = (Triangle*)sta_allocate_struct(Triangle, number_of_triangles);
  u32       triangle_count = 0;

  triangulate_earclipping(triangles, triangle_count, v_points, point_count, count, number_of_triangles);

  align_text(triangles, triangle_count, alignment_x, alignment_y, font_size * this->font->scale, x, y);

  sta_glBindVertexArray(this->text_buffer.vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, this->text_buffer.vbo);
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(Triangle) * triangle_count, triangles, GL_DYNAMIC_DRAW);

  u32 index_count = triangle_count * 3;
  u32 triangle_indices[index_count];
  for (u32 i = 0; i < index_count; i++)
  {
    triangle_indices[i] = i;
  }

  sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->text_buffer.ebo);
  sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * index_count, triangle_indices, GL_DYNAMIC_DRAW);

  this->text_shader.use();
  this->text_shader.set_float4f("color", (float*)&color);

  sta_deallocate(v_points, sizeof(Vector2**) * v_cap);
  sta_deallocate(point_count, sizeof(u32) * p_cap);

  glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, 0);
}
void Renderer::bind_texture(u32 texture_id, u32 texture_unit)
{
  glActiveTexture(texture_unit);
  sta_glBindTexture(GL_TEXTURE_2D, texture_id);
}
u32 Renderer::create_texture(u32 width, u32 height, void* data)
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
