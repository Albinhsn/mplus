#ifndef RENDERER_H
#define RENDERER_H

#include "collision.h"
#include "common.h"
#include "files.h"
#include "font.h"
#include "sdl.h"
#include "shader.h"

enum TextAlignment
{
  TextAlignment_Start_At,
  TextAlignment_End_At,
  TextAlignment_Centered
};

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
  Font*          font;
  Logger*        logger;
  GLBufferIndex  quad_buffer_2d;
  Shader         quad_shader;
  Shader         text_shader;
  GLBufferIndex  text_buffer;
  u32            screen_width;
  u32            screen_height;
  GLBufferIndex* index_buffers;
  u32            index_buffers_count;
  u32            index_buffers_cap;
  Renderer(u32 screen_width, u32 screen_height, Font* font, Logger* logger)
  {
    sta_init_sdl_gl(&window, &context, screen_width, screen_height);
    this->screen_width  = screen_width;
    this->screen_height = screen_height;
    this->font          = font;
    this->logger        = logger;
    this->init_quad_buffer_2d();
    this->init_text_shader();
  }

  SDL_Window*   window;
  SDL_GLContext context;
  u32           create_texture(u32 width, u32 height, void* data);
  void          bind_texture(u32 texture_id, u32 texture_unit);
  void          clear_framebuffer();
  void          swap_buffers();
  void          enable_2d_rendering();
  void          disable_2d_rendering();
  u32           create_buffer_indices(u64 buffer_size, void* buffer_data, u64 index_count, u32* indices, BufferAttributes* attributes, u32 attribute_count);
  u32           create_buffer(u64 buffer_size, void* buffer_data, BufferAttributes* attributes, u64 attribute_count);
  void          render_arrays(u32 buffer_id, GLenum type, u32 count);
  void          render_buffer(u32 buffer_id);
  void          render_2d_quad(f32 min[2], f32 max[2], Color color);
  void          draw_line(f32 x1, f32 y1, f32 x2, f32 y2);
  void          render_text(const char* string, u32 string_length, f32 x, f32 y, TextAlignment alignment_x, TextAlignment alignment_y, Color color, f32 font_size);
  void          toggle_wireframe_on();
  void          toggle_wireframe_off();
  void          change_screen_size(u32 screen_width, u32 screen_height);

private:
  void init_quad_buffer_2d();
  void init_text_shader();
};

#endif
