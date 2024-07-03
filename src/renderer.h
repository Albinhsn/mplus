#ifndef RENDERER_H
#define RENDERER_H

#include "collision.h"
#include "animation.h"
#include "common.h"
#include "files.h"
#include "font.h"
#include "sdl.h"
#include "shader.h"

struct RenderBuffer
{
  const char* model_name;
  u32         buffer_id;
};

enum ModelFileExtensions
{
  MODEL_FILE_OBJ,
  MODEL_FILE_GLB,
  MODEL_FILE_UNKNOWN,
};

enum TextAlignment
{
  TextAlignment_Start_At,
  TextAlignment_End_At,
  TextAlignment_Centered
};

enum BufferAttributeType
{
  BUFFER_ATTRIBUTE_FLOAT,
  BUFFER_ATTRIBUTE_INT,
};

struct BufferAttributes
{

  u8                  count;
  BufferAttributeType type;
};

struct GLBufferIndex
{
  GLuint vao, vbo, ebo;
  GLuint index_count;
};

struct Texture
{
  const char* name;
  i32         id;
  i32         unit;
};

struct Renderer
{
public:
  bool           vsync;
  Texture*       textures;
  u32            texture_count;
  u64            used_texture_units;
  AFont*         font;
  Shader         text_shader;
  Shader         circle_shader;
  Shader*        shaders;
  u32            shader_count;
  GLBufferIndex  text_buffer;
  GLBufferIndex  circle_buffer;
  u32            screen_width;
  u32            screen_height;
  GLBufferIndex* index_buffers;
  u32            index_buffers_count;
  u32            index_buffers_cap;
  u32            line_vao;
  u32            line_vbo;
  Logger*        logger;
  Model*         models;
  u32            model_count;
  RenderBuffer*  buffers;
  u32            buffer_count;
  SDL_Window*    window;
  SDL_GLContext  context;
  Renderer(u32 screen_width, u32 screen_height, AFont* font, Logger* logger, bool vsync)
  {
    this->vsync  = !vsync;
    this->logger = logger;
    sta_init_sdl_gl(&window, &context, screen_width, screen_height, this->vsync);
    this->screen_width  = screen_width;
    this->screen_height = screen_height;
    this->font          = font;
    this->init_line_buffer();
    this->init_circle_buffer();
    this->index_buffers_cap   = 0;
    this->index_buffers_count = 0;
    this->texture_count             = 0;
    this->used_texture_units        = 0;
  }

  void clear_framebuffer();
  void swap_buffers();

  // manage some buffer
  u32     create_buffer_indices(u64 buffer_size, void* buffer_data, u64 index_count, u32* indices, BufferAttributes* attributes, u32 attribute_count);
  u32     create_buffer(u64 buffer_size, void* buffer_data, BufferAttributes* attributes, u64 attribute_count);
  u32     create_texture(u32 width, u32 height, void* data);
  void    bind_texture(Shader shader, const char* uniform_name, u32 texture_index);
  u32     create_buffer_from_model(Model* model, BufferAttributes* attributes, u32 attribute_count);
  bool    load_shaders_from_files(const char* file_location);
  bool    load_textures_from_files(const char* file_location);
  bool    load_buffers_from_files(const char* file_location);
  bool    load_models_from_files(const char* file_location);

  u32     get_texture(const char* name);
  Shader* get_shader_by_name(const char* name);
  Model*  get_model_by_filename(const char* filename);
  u32     get_buffer_by_filename(const char* filename);
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
