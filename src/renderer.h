#ifndef RENDERER_H
#define RENDERER_H

#include "common.h"
#include "files.h"
#include "sdl.h"

struct BufferAttributes
{

  u8  count;
  u32 type;
};

struct GLBufferIndex
{
  GLuint vao, vbo, ebo;
  GLuint index_count;
};

struct Renderer
{
public:
  u32            screen_width;
  u32            screen_height;
  GLBufferIndex* index_buffers;
  u32            index_buffers_count;
  u32            index_buffers_cap;
  Renderer(u32 screen_width, u32 screen_height)
  {
    sta_init_sdl_gl(&window, &context, screen_width, screen_height);
    this->screen_width  = screen_width;
    this->screen_height = screen_height;
  }
  SDL_Window*   window;
  SDL_GLContext context;
  u32           generate_texture(u32 width, u32 height, void* data);
  void          bind_texture(u32 texture_id, u32 texture_unit);
  void          clear_framebuffer();
  void          draw();
  u32           create_buffer(u64 buffer_size, void* buffer_data, u64 index_count, u32* indices, BufferAttributes* attributes, u32 attribute_count);
  void          render_buffer(u32 buffer_id);
};

#endif
