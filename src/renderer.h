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

// init textures via a list of strings
// generate the textures and store them in unit if space is possible
// send back the index of a texture, when generating we can still query the hashmap for the texture
// so bind texture takes the name/index of a texture
struct Texture
{
  const char* name;
  i32         id;
  i32         unit;
};

struct Renderer
{
public:
  bool vsync;
  Texture*       textures;
  u32            texture_count;
  u64            used_texture_units;
  u32            texture_capacity;
  AFont*         font;
  Logger*        logger;
  Shader         text_shader;
  Shader         circle_shader;
  GLBufferIndex  text_buffer;
  GLBufferIndex  circle_buffer;
  u32            screen_width;
  u32            screen_height;
  GLBufferIndex* index_buffers;
  u32            index_buffers_count;
  u32            index_buffers_cap;
  u32            line_vao;
  u32            line_vbo;
  SDL_Window*    window;
  SDL_GLContext  context;
  Renderer(u32 screen_width, u32 screen_height, AFont* font, Logger* logger, bool vsync)
  {
    this->vsync = !vsync;
    sta_init_sdl_gl(&window, &context, screen_width, screen_height, this->vsync);
    this->screen_width  = screen_width;
    this->screen_height = screen_height;
    this->font          = font;
    this->logger        = logger;
    this->init_line_buffer();
    this->init_circle_buffer();
    this->index_buffers_cap   = 0;
    this->index_buffers_count = 0;
    texture_count             = 0;
    used_texture_units        = 0;
    texture_capacity          = 0;
  }

  void clear_framebuffer();
  void swap_buffers();

  // manage some buffer
  u32  create_buffer_indices(u64 buffer_size, void* buffer_data, u64 index_count, u32* indices, BufferAttributes* attributes, u32 attribute_count);
  u32  create_buffer(u64 buffer_size, void* buffer_data, BufferAttributes* attributes, u64 attribute_count);
  u32  create_texture(u32 width, u32 height, void* data);
  void bind_texture(Shader shader, const char* uniform_name, u32 texture_index);
  bool load_textures_from_files(const char* file_location);
  u32  get_texture(const char* name);

  // render some buffer
  void render_arrays(u32 buffer_id, GLenum type, u32 count);
  void render_buffer(u32 buffer_id);
  void render_text(const char* string, u32 string_length, f32 x, f32 y, TextAlignment alignment_x, TextAlignment alignment_y, Color color, f32 font_size);

  // change some context
  void toggle_wireframe_on();
  void toggle_vsync();
  void toggle_wireframe_off();
  void change_screen_size(u32 screen_width, u32 screen_height);
  void enable_2d_rendering();
  void disable_2d_rendering();

  // draw basic primitives
  void draw_circle(Vector2 position, f32 radius, f32 thickness, Color color);
  void draw_line(f32 x1, f32 y1, f32 x2, f32 y2, u32 line_width, Color color);

private:
  void init_line_buffer();
  void init_circle_buffer();
  u32  get_free_texture_unit();
};

#endif
