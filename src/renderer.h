#ifndef RENDERER_H
#define RENDERER_H

#include "collision.h"
#include "animation.h"
#include "common.h"
#include "files.h"
#include "font.h"
#include "platform.h"
#include "sdl.h"
#include "shader.h"

struct EntityRenderData
{
public:
  Mat44 get_model_matrix()
  {
    Mat44 m = Mat44::identity();
    m = m.scale(scale);
    if (rotation_x)
    {
      m = m.rotate_x(rotation_x);
    }
    return m;
  }
  char*                name;
  AnimationController* animation_controller;
  u32                  texture;
  u32                  shader;
  f32                  scale;
  f32                  rotation_x;
  u32                  buffer_id;
};

struct RenderBuffer
{
  const char* model_name;
  u32         buffer_id;
};

enum ModelFileExtensions
{
  MODEL_FILE_OBJ,
  MODEL_FILE_GLB,
  MODEL_FILE_ANIM,
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

struct RenderQueueItemAnimated
{
  u32    buffer;
  Mat44  m;
  Mat44* transforms;
  u32    joint_count;
  u32 texture;
};
struct RenderQueueItemStatic
{
  u32   buffer;
  Mat44 m;
  u32 texture;
};

struct Renderer
{
public:
  RenderQueueItemAnimated* render_queue_animated_buffers;
  u32                      render_queue_animated_count;
  u32                      render_queue_animated_capacity;

  RenderQueueItemStatic*   render_queue_static_buffers;
  u32                      render_queue_static_count;
  u32                      render_queue_static_capacity;

  Mat44                    light_space_matrix;

  void                     push_render_item_animated(u32 buffer, Mat44 m, Mat44* transforms, u32 joint_count, u32 texture);
  void                     push_render_item_static(u32 buffer, Mat44 m, u32 texture);
  void                     render_to_depth_texture_directional(Vector3 light_direction);
  void                     render_to_depth_texture_cube(Vector3 light_position, u32 buffer);
  void                     render_queues(Mat44 view, Vector3 view_position, Mat44 projection, Vector3 light_position, u32 cube_map, Vector3 directional_light_direction);

  u32                      line_vao, line_vbo;
  u32                      shadow_width, shadow_height;
  GLuint                   shadow_map_framebuffer;
  u32                      depth_texture;

  Texture*                 textures;
  u32                      texture_count;
  u32                      texture_capacity;
  RenderBuffer*            buffers;
  u32                      buffer_count;
  Shader*                  shaders;
  u32                      shader_count;

  Shader                   circle_shader;
  u64                      used_texture_units;

  GLBufferIndex            circle_buffer;
  GLBufferIndex*           index_buffers;
  u32                      index_buffers_count;
  u32                      index_buffers_cap;

  Logger*                  logger;

  u32                      screen_width, screen_height;
  bool                     vsync;
  SDL_Window*              window;
  SDL_GLContext            context;
  Renderer()
  {
  }
  Renderer(u32 screen_width, u32 screen_height, Logger* logger, bool vsync)
  {
    this->vsync  = !vsync;
    this->logger = logger;
    sta_init_sdl_gl(&window, &context, screen_width, screen_height, this->vsync);
    this->screen_width  = screen_width;
    this->screen_height = screen_height;
    this->init_circle_buffer();
    this->init_line_buffer();
    this->index_buffers_cap              = 0;
    this->index_buffers_count            = 0;
    this->texture_count                  = 0;
    this->used_texture_units             = 0;
    this->render_queue_static_count      = 0;
    this->render_queue_static_capacity   = 2;
    this->render_queue_static_buffers    = sta_allocate_struct(RenderQueueItemStatic, this->render_queue_static_capacity);
    this->render_queue_animated_count    = 0;
    this->render_queue_animated_capacity = 2;
    this->render_queue_animated_buffers  = sta_allocate_struct(RenderQueueItemAnimated, this->render_queue_animated_capacity);
  }

  // manage some buffer
  void    init_depth_texture();
  u32     create_buffer_indices(u64 buffer_size, void* buffer_data, u64 index_count, u32* indices, BufferAttributes* attributes, u32 attribute_count);
  u32     create_buffer(u64 buffer_size, void* buffer_data, BufferAttributes* attributes, u64 attribute_count);
  u32     create_texture(u32 width, u32 height, void* data);
  void    bind_texture(Shader shader, const char* uniform_name, u32 texture_index);
  void    bind_cube_texture(Shader shader, const char* uniform_name, u32 texture_index);
  u32     create_buffer_from_model(Model* model, BufferAttributes* attributes, u32 attribute_count);
  bool    load_shaders_from_files(const char* file_location);
  bool    load_textures_from_files(const char* file_location);
  u32     add_texture(u32 texture_id);
  void    reload_shaders();
  void    draw_line(f32 x1, f32 y1, f32 x2, f32 y2, u32 line_width, Color color);
  void    init_line_buffer();

  u32     get_texture(const char* name);
  Shader* get_shader_by_index(u32 index);
  u32     get_shader_by_name(const char* name);

  // render some buffer
  void render_buffer(u32 buffer_id);
  void render_text(const char* string, u32 string_length, f32 x, f32 y, TextAlignment alignment_x, TextAlignment alignment_y, Color color, f32 font_size);

  // change some context
  void toggle_wireframe_on();
  void toggle_vsync();
  void toggle_wireframe_off();
  void change_screen_size(u32 screen_width, u32 screen_height);
  void enable_2d_rendering();
  void disable_2d_rendering();
  void change_viewport(u32 w, u32 h);
  void reset_viewport_to_screen_size();
  void clear_framebuffer();
  void swap_buffers();

  // draw basic primitives
  void draw_circle(Vector2 position, f32 radius, f32 thickness, Color color, Mat44 view, Mat44 projection);

private:
  void init_circle_buffer();
  u32  get_free_texture_unit();
};

#endif
