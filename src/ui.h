#ifndef UI_H
#define UI_H
#include "common.h"
#include "input.h"
#include "platform.h"
#include "renderer.h"
#include "vector.h"

enum UI_SizeType
{
  UI_SIZE_PIXELS,
  UI_SIZE_TEXTCONTENT,
  UI_SIZE_PERCENT_OF_PARENT,
  UI_SIZE_CHILDREN_SUM
};

struct UI_Size
{
  UI_SizeType type;
  f32         value;
  f32         strictness;
};

enum Axis2
{
  Axis2_X,
  Axis2_Y,
  Axis2_COUNT
};

struct UI_Key
{
};

typedef u32 UI_WidgetFlags;
enum
{
  UI_WidgetFlag_Clickable       = (1 << 0),
  UI_WidgetFlag_ViewScroll      = (1 << 1),
  UI_WidgetFlag_DrawText        = (1 << 2),
  UI_WidgetFlag_DrawBorder      = (1 << 3),
  UI_WidgetFlag_DrawBackground  = (1 << 4),
  UI_WidgetFlag_DrawDropShadow  = (1 << 5),
  UI_WidgetFlag_Clip            = (1 << 6),
  UI_WidgetFlag_HotAnimation    = (1 << 7),
  UI_WidgetFlag_ActiveAnimation = (1 << 8),
};

struct UI_Widget
{
public:
  UI_Widget(f32 x[2], f32 y[2], UI_WidgetFlags flags, String string, Color color)
  {
    this->x[0] = x[0];
    this->x[1] = x[1];

    this->y[0] = y[0];
    this->y[1] = y[1];
    this->background = color;
  }
  UI_Widget*     first;
  UI_Widget*     last;
  UI_Widget*     next;
  UI_Widget*     prev;
  UI_Widget*     parent;

  UI_Widget*     hash_next;
  UI_Widget*     hash_prev;

  UI_Key         key;
  u64            last_frame_touched_index;

  UI_WidgetFlags flags;
  UI_Size        semantic_size[Axis2_COUNT];
  String         string;

  Color          background;
  f32            computed_relative_position[Axis2_COUNT];
  f32            computed_size[Axis2_COUNT];
  f32            x[2];
  f32            y[2];

  f32            hot_t;
  f32            active_t;
};

struct UI_Comm
{
  UI_Widget* widget;
  Vector2    mouse;
  Vector2    drag_delta;
  bool       left_clicked;
  bool       double_clicked;
  bool       right_clicked;
  bool       pressed;
  bool       released;
  bool       dragging;
  bool       hovering;
};

struct UI_Layout
{
public:
  UI_Layout(f32 min[2], f32 max[2])
  {
    this->min[0] = min[0];
    this->min[1] = min[1];

    this->max[0] = max[0];
    this->max[1] = max[1];
  }
  f32 min[2];
  f32 max[2];
};

struct UI
{
public:
  UI(Renderer* renderer, InputState* input)
  {
    this->renderer     = renderer;
    this->input        = input;
    this->layouts      = (UI_Layout*)sta_allocate_struct(UI_Layout, 1);
    this->layout_count = 0;
    this->layout_cap   = 1;
  }
  UI_Layout* layouts;
  u32        layout_count;
  u32        layout_cap;

  UI_Layout current_layout();
  void       add_layout(f32 min[2], f32 max[2]);
  UI_Comm    comm_from_widget(UI_Widget* widget);
  UI_Comm    UI_Button(String string, f32 min[2], f32 max[2]);

private:
  Renderer*   renderer;
  InputState* input;
};

#endif
