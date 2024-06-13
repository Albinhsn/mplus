#ifndef STA_RENDERER_H
#define STA_RENDERER_H

#include "common.h"
#include "files.h"
#include "sdl.h"
#include "vector.h"

struct FrameBuffer
{
  u64  height;
  u64  width;
  u8*  buffer;
  i32* z_buffer;
};

struct Renderer
{
  SDL_Window* window;
  FrameBuffer buffer;
};

void init_renderer(Renderer* renderer, u64 screen_width, u64 screen_height);
void clear_buffer(Renderer* renderer, Color color);
void render(Renderer* renderer);

void draw_line(FrameBuffer* buffer, u64 startX, u64 startY, u64 endX, u64 endY, Color color, u64 thickness);
void draw_line(FrameBuffer* buffer, u64 startX, u64 startY, u64 endX, u64 endY, Color color);
void set_pixel(FrameBuffer* buffer, u64 x, u64 y, Color color);

void draw_triangle(FrameBuffer* buffer, u64 v0[2], u64 v1[2], u64 v2[2], Color color);
void draw_triangle_unfilled(FrameBuffer* buffer, u64 v0[2], u64 v1[2], u64 v2[2], Color color, u64 thickness);

void draw_rectangle_unfilled(FrameBuffer* buffer, u64 v0[2], u64 v1[2], u64 v2[2], u64 v3[2], Color color, u64 thickness);
void draw_rectangle_unfilled(FrameBuffer* buffer, u16 x0, u16 x1, u16 y0, u16 y1, Color color, u64 thickness);

void draw_rectangle(FrameBuffer* buffer, Image* image, u64 v0[2], u64 v1[2], u64 v2[2], u64 v3[2]);
void draw_rectangle(FrameBuffer* buffer, Image* image, u8 x0, u8 x1, u8 y0, u8 y1);
void draw_rectangle(FrameBuffer* buffer, u64 v0[2], u64 v1[2], u64 v2[2], u64 v3[2], Color color);
void draw_rectangle(FrameBuffer* buffer, u16 x0, u16 x1, u16 y0, u16 y1, Color color);

void draw_circle(FrameBuffer* buffer, u64 x, u64 y, u64 r, Color color);
void draw_circle_unfilled(FrameBuffer* buffer, u64 x, u64 y, u64 r, Color color);
void draw_cubic_bezier(FrameBuffer* buffer, u64 p0[2], u64 p1[2], u64 p2[2], u64 p3[2], Color color, u64 samples);

bool parse_wavefront_object(ModelData* model, const char* filename);
f32  parseFloatFromString(const char* source, u8* length);
bool convert_obj_to_model(const char* input_filename, const char* output_filename);

#endif
