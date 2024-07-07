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
void Renderer::change_viewport(u32 w, u32 h)
{
  glViewport(0, 0, w, h);
}
void Renderer::reset_viewport_to_screen_size()
{
  glViewport(0, 0, screen_width, screen_height);
}

void Renderer::init_depth_texture()
{
  shadow_width = 1024, shadow_height = 1024;
  sta_glGenFramebuffers(1, &this->shadow_map_framebuffer);

  GLuint depth_texture;
  glGenTextures(1, &depth_texture);
  glBindTexture(GL_TEXTURE_2D, depth_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadow_width, shadow_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  float borderColor[] = {1.0, 1.0, 1.0, 1.0};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
  sta_glBindFramebuffer(GL_FRAMEBUFFER, this->shadow_map_framebuffer);
  sta_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  sta_glBindFramebuffer(GL_FRAMEBUFFER, 0);
  this->depth_texture = this->add_texture(depth_texture);
}

void Renderer::change_screen_size(u32 screen_width, u32 screen_height)
{
  SDL_SetWindowSize(this->window, screen_width, screen_height);
  glViewport(0, 0, screen_width, screen_height);
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
  f32    lines[4] = {x1, y1, x2, y2};
  Shader s        = *this->get_shader_by_index(this->get_shader_by_name("quad"));
  s.use();
  s.set_float4f("color", (float*)&color);

  sta_glBindVertexArray(this->line_vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, this->line_vbo);
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * 4, lines, GL_DYNAMIC_DRAW);
  glLineWidth(line_width);
  glDrawArrays(GL_LINES, 0, 2);
  this->disable_2d_rendering();
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

void Renderer::draw_circle(Vector2 position, f32 radius, f32 thickness, Color color, Mat44 view, Mat44 projection)
{
  this->enable_2d_rendering();
  this->circle_shader.use();
  this->circle_shader.set_float("thickness", thickness);
  this->circle_shader.set_float4f("color", (float*)&color);
  this->circle_shader.set_mat4("view", view);
  this->circle_shader.set_mat4("projection", projection);
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

u32 Renderer::add_texture(u32 texture_id)
{
  RESIZE_ARRAY(this->textures, Texture, texture_count, texture_capacity);
  u32      out     = texture_count++;
  Texture* texture = &this->textures[out];
  texture->id      = texture_id;
  texture->unit    = this->get_free_texture_unit();
  return out;
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
  this->logger->error("Couldn't find texture '%s'", name);
  assert(!"Didn't find texture!");
}
Shader* Renderer::get_shader_by_index(u32 index)
{
  return &this->shaders[index];
}

void Renderer::reload_shaders()
{
  for (u32 i = 0; i < shader_count; i++)
  {
    Shader* shader = &shaders[i];
    if (!recompile_shader(shader))
    {
      logger->error("Failed to recompile '%s'", shader->name);
    }
  }
}

u32 Renderer::get_shader_by_name(const char* name)
{
  for (u32 i = 0; i < this->shader_count; i++)
  {
    if (compare_strings(name, this->shaders[i].name))
    {
      return i;
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

  u32     count   = parse_int_from_string(lines.strings[0]);

  Shader* shaders = sta_allocate_struct(Shader, count);

  for (u32 i = 0, string_index = 1; i < count; i++)
  {
    char log[256];
    memset(log, 0, ArrayCount(log));
    u64         log_index    = 0;
    const char* name         = lines.strings[string_index++];
    u32         shader_count = parse_int_from_string(lines.strings[string_index++]);
    ShaderType  types[shader_count];
    char*       shader_locations[shader_count];

    log_index = snprintf(log, ArrayCount(log), "Found shader '%s': ", name);
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

      log_index += snprintf(&log[log_index], ArrayCount(log) - log_index, "type: %d at '%s',\t", type, file_location);
      assert(log_index < ArrayCount(log) && "Overflow in log msg!");
      shader_locations[j] = file_location;
      types[j]            = type;
    }

    this->logger->info(log);

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

  this->texture_count    = parse_int_from_string(lines.strings[0]);
  this->textures         = sta_allocate_struct(Texture, texture_count);
  this->texture_capacity = this->texture_count;

  for (u32 i = 0, string_index = 1; i < this->texture_count; i++)
  {
    Texture    texture = {};
    TargaImage image   = {};
    texture.name       = lines.strings[string_index++];

    char* targa_file   = lines.strings[string_index++];
    if (!sta_targa_read_from_file_rgba(&image, targa_file))
    {
      // ToDo load token texture
      this->logger->error("Failed to read targa from '%s'", targa_file);
    }
    texture.id   = this->create_texture(image.width, image.height, image.data);
    texture.unit = this->get_free_texture_unit();
    sta_deallocate(image.data, image.width * image.height * 4);

    this->textures[i] = texture;
  }

  logger->info("Found %d textures", this->texture_count);
  for (u32 i = 0; i < this->texture_count; i++)
  {
    Texture texture = this->textures[i];
    logger->info("texture: '%s' id:%d, unit:%d", texture.name, texture.id, texture.unit);
  }

  // ToDo free the line memory in every load x
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
    case BUFFER_ATTRIBUTE_FLOAT:
    {
      sta_glVertexAttribPointer(i, attribute.count, GL_FLOAT, GL_FALSE, size, (void*)(stride * sizeof(int)));
      break;
    }
    case BUFFER_ATTRIBUTE_INT:
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
    case BUFFER_ATTRIBUTE_FLOAT:
    {
      sta_glVertexAttribPointer(i, attribute.count, GL_FLOAT, GL_FALSE, size, (void*)(stride * sizeof(int)));
      break;
    }
    case BUFFER_ATTRIBUTE_INT:
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
