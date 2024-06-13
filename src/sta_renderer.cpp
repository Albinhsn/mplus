#include "sta_renderer.h"
#include "common.h"
#include "files.h"
#include "platform.h"
#include "vector.h"
#include <climits>
#include <cmath>
#include <cstring>

void set_pixel(FrameBuffer* buffer, u64 x, u64 y, ColorU8 color)
{
  if (x >= buffer->width || x < 0 || y >= buffer->height || y < 0)
  {
    return;
  }
  u64 index                 = (x + y * buffer->width) * 4;
  buffer->buffer[index + 0] = color.r;
  buffer->buffer[index + 1] = color.g;
  buffer->buffer[index + 2] = color.b;
  buffer->buffer[index + 3] = color.a;
}

void set_pixel(FrameBuffer* buffer, u64 x, u64 y, Color color)
{
  u64 index                 = (x + y * buffer->width) * 4;
  buffer->buffer[index + 0] = 255 * color.r;
  buffer->buffer[index + 1] = 255 * color.g;
  buffer->buffer[index + 2] = 255 * color.b;
  buffer->buffer[index + 3] = 255 * color.a;
}

void init_renderer(Renderer* renderer, u64 screen_width, u64 screen_height)
{
  renderer->buffer.width    = screen_width;
  renderer->buffer.height   = screen_height;
  renderer->buffer.z_buffer = (i32*)sta_allocate_struct(i32, screen_width * screen_height);
  memset(renderer->buffer.z_buffer, INT_MIN, screen_width * screen_height);
  sta_init_sdl_window(&renderer->buffer.buffer, &renderer->window, screen_width, screen_height);
}

void clear_buffer(Renderer* renderer, Color color)
{
  u64  buffer_size = renderer->buffer.width * renderer->buffer.height;
  bool z_buffer    = renderer->buffer.z_buffer != 0;
  for (u64 i = 0; i < buffer_size; i++)
  {
    u64 pixel_index                          = i * 4;
    renderer->buffer.buffer[pixel_index + 0] = 255 * color.r;
    renderer->buffer.buffer[pixel_index + 1] = 255 * color.g;
    renderer->buffer.buffer[pixel_index + 2] = 255 * color.b;
    renderer->buffer.buffer[pixel_index + 3] = 255 * color.a;
    if (z_buffer)
    {
      renderer->buffer.z_buffer[i] = INT_MIN;
    }
  }
}

void render(Renderer* renderer)
{
  SDL_UpdateWindowSurface(renderer->window);
  clear_buffer(renderer, BLACK);
}

void draw_line(FrameBuffer* buffer, u64 startX, u64 startY, u64 endX, u64 endY, Color color)
{
  bool steep = ABS((i64)(startX - endX)) < ABS((i64)(startY - endY));
  if (steep)
  {
    u64 tmp = startX;
    startX  = startY;
    startY  = tmp;

    tmp     = endX;
    endX    = endY;
    endY    = tmp;
  }

  if (startX > endX)
  {
    u64 tmp = startX;
    startX  = endX;
    endX    = tmp;

    tmp     = startY;
    startY  = endY;
    endY    = tmp;
  }

  i64  dx        = endX - startX;
  i64  dy        = 2 * (endY - startY);

  bool downwards = dy < 0;
  dy             = downwards ? -dy : dy;

  i64 derror     = 0;
  u64 y          = startY;
  for (u64 x = startX; x < endX; x++)
  {
    set_pixel(buffer, steep ? y : x, steep ? x : y, color);
    derror += dy;
    if (derror > dx)
    {
      y = downwards ? y - 1 : y + 1;
      derror -= 2 * dx;
    }
  }
}
void draw_line(FrameBuffer* buffer, u64 startX, u64 startY, u64 endX, u64 endY, Color color, u64 thickness)
{
  bool steep = ABS((i64)(startX - endX)) < ABS((i64)(startY - endY));
  if (steep)
  {
    u64 tmp = startX;
    startX  = startY;
    startY  = tmp;

    tmp     = endX;
    endX    = endY;
    endY    = tmp;
  }

  if (startX > endX)
  {
    u64 tmp = startX;
    startX  = endX;
    endX    = tmp;

    tmp     = startY;
    startY  = endY;
    endY    = tmp;
  }

  i64  dx            = endX - startX;
  i64  dy            = endY - startY;

  i64  d             = 2 * dy;

  bool downwards     = d < 0;
  d                  = downwards ? -d : d;

  i64 derror         = 0;
  u64 y              = startY;
  int thickness_half = thickness / 2;

  for (u64 x = startX; x < endX; x++)
  {
    for (i64 i = -thickness_half; i < thickness_half; i++)
    {
      if (steep)
      {
        i64 pixel_x = y + (downwards ? 0 : i);
        i64 pixel_y = x + (downwards ? i : 0);
        set_pixel(buffer, pixel_x, pixel_y, color);
      }
      else
      {
        i64 pixel_x = x + (downwards ? i : 0);
        i64 pixel_y = y + (downwards ? 0 : i);
        set_pixel(buffer, pixel_x, pixel_y, color);
      }
    }

    derror += d;
    if (derror > dx)
    {
      y = downwards ? y - 1 : y + 1;
      derror -= 2 * dx;
    }
  }
}

inline void draw_line(FrameBuffer* buffer, u64 start[2], u64 end[2], Color color, u64 thickness)
{
  draw_line(buffer, start[0], start[1], end[0], end[1], color, thickness);
}

void draw_line(FrameBuffer* buffer, u64 start[2], u64 end[2], Color color)
{
  draw_line(buffer, start[0], start[1], end[0], end[1], color);
}

void draw_cubic_bezier(FrameBuffer* buffer, u64 p0[2], u64 p1[2], u64 p2[2], u64 p3[2], Color color, u64 samples, int thickness)
{

  u64 prev_x = p0[0];
  u64 prev_y = p0[1];
  for (u64 sample = 0; sample < samples; sample++)
  {
    f64 t         = sample / (f64)samples;
    f64 t_squared = t * t;
    f64 t0        = pow(1 - t, 3);
    f64 t1        = 3 * pow(1 - t, 2) * t;
    f64 t2        = 3 * (1 - t) * t_squared;
    f64 t3        = t * t_squared;

    u64 x         = p0[0] * t0 + p1[0] * t1 + p2[0] * t2 + t3 * p3[0];
    u64 y         = p0[1] * t0 + p1[1] * t1 + p2[1] * t2 + t3 * p3[1];
    draw_line(buffer, x, y, prev_x, prev_y, color, thickness);

    prev_x = x;
    prev_y = y;
  }
}

void draw_circle(FrameBuffer* buffer, u64 orig_x, u64 orig_y, i64 r, Color color)
{
  i64 r_squared = r * r;
  for (i64 x = -r; x <= r; x++)
  {
    i64 x_diff = x * x;
    for (i64 y = -r; y <= r && x_diff + y * y <= r_squared; y++)
    {
      set_pixel(buffer, x + orig_x, y + orig_y, color);
    }
  }
}

void draw_circle_unfilled(FrameBuffer* buffer, u64 x, u64 y, u64 r, Color color)
{
  f64 a = 1.00005519;
  f64 b = 0.55342686;
  f64 c = 0.99873585;

  for (u64 i = 0; i <= 1; i++)
  {
    for (u64 j = 0; j <= 1; j++)
    {
      i64 r1    = j ? r : -r;
      i64 r0    = i ? r : -r;
      u64 p0[2] = {};
      p0[0]     = x;
      p0[1]     = a * r0 + y;

      u64 p1[2] = {};
      p1[0]     = b * r1 + x;
      p1[1]     = c * r0 + y;

      u64 p2[2] = {};
      p2[0]     = c * r1 + x;
      p2[1]     = b * r0 + y;

      u64 p3[2] = {};
      p3[0]     = a * r1 + x;
      p3[1]     = y;
      draw_cubic_bezier(buffer, p0, p1, p2, p3, color, 250, 2);
    }
  }
}

void draw_circle(FrameBuffer* buffer, u64 x, u64 y, u64 r, Color color, int thickness)
{
}

void draw_cubic_bezier(FrameBuffer* buffer, u64 p0[2], u64 p1[2], u64 p2[2], u64 p3[2], Color color, u64 samples)
{
  u64 prev_x = p0[0];
  u64 prev_y = p0[1];
  for (u64 sample = 0; sample < samples; sample++)
  {
    f64 t         = sample / (f64)samples;
    f64 t_squared = t * t;
    f64 t0        = pow(1 - t, 3);
    f64 t1        = 3 * pow(1 - t, 2) * t;
    f64 t2        = 3 * (1 - t) * t_squared;
    f64 t3        = t * t_squared;

    u64 x         = p0[0] * t0 + p1[0] * t1 + p2[0] * t2 + t3 * p3[0];
    u64 y         = p0[1] * t0 + p1[1] * t1 + p2[1] * t2 + t3 * p3[1];
    draw_line(buffer, x, y, prev_x, prev_y, color);

    prev_x = x;
    prev_y = y;
  }
}
static inline i64 cross_2d(u64 v0[2], u64 v1[2], u64 x, u64 y)
{
  return (v1[0] - v0[0]) * (y - v0[1]) * (v1[1] - v0[1]) * (x - v0[0]);
}

void draw_triangle(FrameBuffer* buffer, u64 v0[2], u64 v1[2], u64 v2[2], Color color)
{

  u16 min_x = MIN(MIN(v0[0], v1[0]), v2[0]);
  u16 max_x = MAX(MAX(v0[0], v1[0]), v2[0]);
  u16 min_y = MIN(MIN(v0[1], v1[1]), v2[1]);
  u16 max_y = MAX(MAX(v0[1], v1[1]), v2[1]);

  for (u16 x = min_x; x < max_x; x++)
  {
    for (u16 y = min_y; y < max_y; y++)
    {

      i64  w0     = cross_2d(v1, v2, x, y);
      i64  w1     = cross_2d(v2, v0, x, y);
      i64  w2     = cross_2d(v0, v1, x, y);

      bool inside = w0 >= 0 && w1 >= 0 && w2 >= 0;

      if (inside)
      {
        set_pixel(buffer, x, y, color);
      }
    }
  }
}

void draw_triangle_unfilled(FrameBuffer* buffer, u64 v0[2], u64 v1[2], u64 v2[2], Color color, u64 thickness)
{
  draw_line(buffer, v0, v1, color, thickness);
  draw_line(buffer, v1, v2, color, thickness);
  draw_line(buffer, v2, v0, color, thickness);
}

void draw_rectangle_unfilled(FrameBuffer* buffer, u64 v0[2], u64 v1[2], u64 v2[2], u64 v3[2], Color color, u64 thickness)
{
  draw_line(buffer, v0, v1, color, thickness);
  draw_line(buffer, v1, v2, color, thickness);
  draw_line(buffer, v2, v3, color, thickness);
  draw_line(buffer, v3, v0, color, thickness);
}
void draw_rectangle_unfilled(FrameBuffer* buffer, u16 x0, u16 y0, u16 x1, u16 y1, Color color, u64 thickness)
{
  draw_line(buffer, x0, y0, x1, y0, color, thickness);
  draw_line(buffer, x1, y0, x1, y1, color, thickness);
  draw_line(buffer, x0, y1, x1, y1, color, thickness);
  draw_line(buffer, x0, y0, x0, y1, color, thickness);
}

void draw_rectangle(FrameBuffer* buffer, Image* image, u64 v0, u64 v1, u64 v2, u64 v3);
void draw_rectangle(FrameBuffer* buffer, Image* image, u16 x0, u16 x1, u16 y0, u16 y1);
void draw_rectangle(FrameBuffer* buffer, u64 v0[2], u64 v1[2], u64 v2[2], u64 v3[2], Color color)
{
}

void draw_rectangle(FrameBuffer* buffer, u16 x0, u16 x1, u16 y0, u16 y1, Color color)
{

  u16 min_x = x0 < x1 ? x0 : x1;
  u16 max_x = x0 > x1 ? x0 : x1;
  u16 min_y = y0 < y1 ? y0 : y1;
  u16 max_y = y0 > y1 ? y0 : y1;
  for (u16 x = min_x; x < max_x; x++)
  {
    for (u16 y = min_y; y < max_y; y++)
    {
      set_pixel(buffer, x, y, color);
    }
  }
}
