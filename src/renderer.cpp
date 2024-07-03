#include "renderer.h"
#include "common.h"
#include "files.h"
#include "platform.h"
#include "sdl.h"
#include "shader.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL_video.h>
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
  SDL_SetWindowSize(this->window, screen_width, screen_height);
  glViewport(0, 0, screen_width, screen_height);
}

void Renderer::init_circle_buffer()
{
  GLBufferIndex* circle_buffer = &this->circle_buffer;
  sta_glGenVertexArrays(1, &circle_buffer->vao);
  sta_glGenBuffers(1, &circle_buffer->vbo);
  sta_glGenBuffers(1, &circle_buffer->ebo);

  sta_glBindVertexArray(circle_buffer->vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, circle_buffer->vbo);
  const int vertices_in_a_quad = 16;
  f32       tot[16]            = {
      1.0f,  1.0f,  1.0f, 1.0f, //
      1.0f,  -1.0,  1.0f, 0.0f, //
      -1.0f, -1.0f, 0.0f, 0.0f, //
      -1.0f, 1.0,   0.0f, 1.0f, //
  };
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * vertices_in_a_quad, tot, GL_DYNAMIC_DRAW);

  sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, circle_buffer->ebo);
  u32 indices[6] = {1, 3, 0, 1, 2, 3};
  sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * ArrayCount(indices), indices, GL_STATIC_DRAW);

  sta_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(f32) * 4, (void*)(0));
  sta_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(f32) * 4, (void*)(2 * sizeof(float)));
  sta_glEnableVertexAttribArray(0);
  sta_glEnableVertexAttribArray(1);

  ShaderType  types[2] = {SHADER_TYPE_VERT, SHADER_TYPE_FRAG};
  const char* names[2] = {"./shaders/circle.vert", "./shaders/circle.frag"};
  this->circle_shader  = Shader(types, names, ArrayCount(types), "circle");
}

void Renderer::draw_circle(Vector2 position, f32 radius, f32 thickness, Color color)
{
  this->enable_2d_rendering();
  this->circle_shader.use();
  this->circle_shader.set_float("thickness", thickness);
  this->circle_shader.set_float4f("color", (float*)&color);
  sta_glBindVertexArray(this->circle_buffer.vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, this->circle_buffer.vbo);
  const int vertices_in_a_quad = 16;
  f32       min_x = position.x - radius, max_x = position.x + radius;
  f32       min_y = position.y - radius, max_y = position.y + radius;

  f32       min_u   = (min_x + 1.0f) / 2.0f;
  f32       max_u   = (max_x + 1.0f) / 2.0f;

  f32       min_v   = (min_y + 1.0f) / 2.0f;
  f32       max_v   = (max_y + 1.0f) / 2.0f;

  f32       tot[16] = {
      max_x, max_y, 1.0f, 1.0f, //
      max_x, min_y, 1.0f, 0.0f, //
      min_x, min_y, 0.0f, 0.0f, //
      min_x, max_y, 0.0f, 1.0f  //
  };
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * vertices_in_a_quad, tot, GL_DYNAMIC_DRAW);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  this->disable_2d_rendering();
}

void Renderer::init_line_buffer()
{
  sta_glGenVertexArrays(1, &this->line_vao);
  sta_glGenBuffers(1, &this->line_vbo);

  sta_glBindVertexArray(this->line_vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, this->line_vbo);
  const int vertices_in_a_line = 4;
  f32       tot[4]             = {
      1.0f, 1.0f, //
      1.0f, -1.0, //
  };
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * vertices_in_a_line, tot, GL_DYNAMIC_DRAW);

  sta_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(f32) * 2, (void*)(0));
  sta_glEnableVertexAttribArray(0);
}

void Renderer::draw_line(f32 x1, f32 y1, f32 x2, f32 y2, u32 line_width, Color color)
{
  this->enable_2d_rendering();
  f32 lines[4] = {x1, y1, x2, y2};
  this->text_shader.use();
  this->text_shader.set_float4f("color", (float*)&color);

  sta_glBindVertexArray(this->line_vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, this->line_vbo);
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * 4, lines, GL_DYNAMIC_DRAW);
  glLineWidth(line_width);
  glDrawArrays(GL_LINES, 0, 2);
  this->disable_2d_rendering();
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
void Renderer::bind_texture(Shader shader, const char* uniform_name, u32 texture_index)
{
  Texture texture = this->textures[texture_index];
  if (texture.unit == -1)
  {
    texture.unit = this->get_free_texture_unit();
  }

  shader.use();
  GLuint location = sta_glGetUniformLocation(shader.id, uniform_name);
  sta_glUniform1i(location, texture.unit);
  glActiveTexture(GL_TEXTURE0 + texture.unit);
  sta_glBindTexture(GL_TEXTURE_2D, texture.id);
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

void Renderer::render_arrays(u32 buffer_id, GLenum type, u32 count)
{
  GLBufferIndex buffer = this->index_buffers[buffer_id];
  sta_glBindVertexArray(buffer.vao);

  glDrawArrays(type, 0, count);
}

u32 Renderer::create_buffer_from_model(Model* model, BufferAttributes* attributes, u32 attribute_count)
{

  return this->create_buffer_indices(model->vertex_data_size * model->vertex_count, model->vertex_data, model->index_count, model->indices, attributes, attribute_count);
}

void Renderer::toggle_vsync()
{
  this->vsync = !this->vsync;
  SDL_GL_SetSwapInterval(this->vsync);
}
u32 Renderer::get_free_texture_unit()
{
  const static u32 MAX_UNITS = 38;
  u64              inv       = ~this->used_texture_units;

  u64              index     = __builtin_ctzll(inv);
  if (index == MAX_UNITS)
  {
    Texture* texture = &textures[0];
    u32      out     = texture->unit;
    texture->unit    = -1;
    return out;
  }
  this->used_texture_units |= index == 0 ? 1 : (1UL << index);
  return index;
}

u32 Renderer::get_texture(const char* name)
{
  for (u32 i = 0; i < this->texture_count; i++)
  {
    if (this->textures[i].id != -1 && compare_strings(name, this->textures[i].name))
    {
      return i;
    }
  }
  assert(!"Didn't find texture!");
}

Shader* Renderer::get_shader_by_name(const char* name)
{
  for (u32 i = 0; i < this->shader_count; i++)
  {
    if (compare_strings(name, this->shaders[i].name))
    {
      return &this->shaders[i];
    }
  }
  logger->error("Didn't find shader '%s'", name);
  assert(!"Couldn't find shader!");
}

bool Renderer::load_shaders_from_files(const char* file_location)
{
  Buffer      buffer = {};
  StringArray lines  = {};
  if (!sta_read_file(&buffer, file_location))
  {
    return false;
  }
  split_buffer_by_newline(&lines, &buffer);
  assert(lines.count > 0 && "No lines in shader file?");

  u32 count = parse_int_from_string(lines.strings[0]);
  logger->info("Found %d shaders at '%s'\n", count, file_location);

  Shader* shaders = sta_allocate_struct(Shader, count);

  for (u32 i = 0, string_index = 1; i < count; i++)
  {
    const char* name         = lines.strings[string_index++];
    u32         shader_count = parse_int_from_string(lines.strings[string_index++]);
    ShaderType  types[shader_count];
    char*       shader_locations[shader_count];

    logger->info("Found shader %s\n", name);
    for (u32 j = 0; j < shader_count; j++)
    {
      char*      line = lines.strings[string_index++];
      Buffer     buffer(line, strlen(line));
      ShaderType type = (ShaderType)buffer.parse_int();
      while (!buffer.is_out_of_bounds() && buffer.current_char() == ' ')
      {
        buffer.advance();
      }

      if (buffer.is_out_of_bounds())
      {
        assert(!"Expected shader file location but found end of line!");
      }

      char* file_location                      = sta_allocate_struct(char, buffer.len - buffer.index + 1);
      file_location[buffer.len - buffer.index] = '\0';
      strncpy(file_location, buffer.current_address(), buffer.len - buffer.index);

      logger->info("Found shader %d: %d %s\n", j, type, file_location);
      shader_locations[j] = file_location;
      types[j]            = type;
    }

    shaders[i] = Shader(types, (const char**)shader_locations, shader_count, name);
  }
  this->shaders      = shaders;
  this->shader_count = count;
  return true;
}

bool Renderer::load_textures_from_files(const char* file_location)
{

  Buffer      buffer = {};
  StringArray lines  = {};
  if (!sta_read_file(&buffer, file_location))
  {
    return false;
  }
  split_buffer_by_newline(&lines, &buffer);

  // ToDo increase?
  const u32 initial_texture_capacity = 4;
  if (this->texture_capacity == 0)
  {
    this->texture_capacity = initial_texture_capacity;
    this->textures         = sta_allocate_struct(Texture, initial_texture_capacity);
  }

  for (u32 i = 0; i < lines.count; i++)
  {
    Texture    texture = {};
    TargaImage image   = {};
    texture.name       = lines.strings[i];

    sta_targa_read_from_file_rgba(&image, texture.name);
    texture.id   = this->create_texture(image.width, image.height, image.data);
    texture.unit = this->get_free_texture_unit();
    sta_deallocate(image.data, image.width * image.height * 4);

    RESIZE_ARRAY(this->textures, Texture, this->texture_count, this->texture_capacity);
    this->textures[this->texture_count++] = texture;
  }

  printf("Found %d textures\n", this->texture_count);
  for (u32 i = 0; i < this->texture_count; i++)
  {
    Texture texture = this->textures[i];
    printf("%s: %d, %d\n", texture.name, texture.id, texture.unit);
  }

  // ToDo free the line memory
  return true;
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
  sta_glBindVertexArray(0);
}

void Renderer::clear_framebuffer()
{
  sta_gl_clear_buffer(0, 0, 0, 1.0f);
}
