#include "ui.h"
#include "common.h"
#include "renderer.h"

UI_Key UI_Key_Null()
{
  return -1;
}
inline bool UI_Key_Compare(UI_Key a, UI_Key b)
{
  return a == b;
}
void UI_Widget_Persitent_Data_Table::add(UI_Key key, UI_Persistent_Data data)
{
  UI_Key null_key = UI_Key_Null();
  u32    i        = 0;
  for (; i < this->widget_key_count; i++)
  {
    if (UI_Key_Compare(this->widget_keys[i], null_key))
    {
      this->persistent_data[i] = data;
      this->widget_keys[i]     = key;
      return;
    }
  }

  RESIZE_ARRAY(this->persistent_data, UI_Persistent_Data, this->widget_key_count, this->widget_key_capacity);
  this->persistent_data[i] = data;
  this->widget_keys[i]     = key;
  this->widget_key_count++;
}
UI_Persistent_Data* UI_Widget_Persitent_Data_Table::get(UI_Key key)
{
  for (u32 i = 0; i < this->widget_key_count; i++)
  {
    if (UI_Key_Compare(key, this->widget_keys[i]))
    {
      return &this->persistent_data[i];
    }
  }
  return 0;
}
void UI_Widget_Persitent_Data_Table::remove(UI_Key key)
{
  for (u32 i = 0; i < this->widget_key_count; i++)
  {
    if (UI_Key_Compare(key, this->widget_keys[i]))
    {
      this->widget_keys[i] = UI_Key_Null();
      this->widget_key_count--;
      return;
    }
  }
  assert(!"Key didn't exist?");
}

UI_Key generate_key(String* s)
{
  return sta_hash_string_fnv(s);
}
static inline i32 find_double_hash_in_string(String* s)
{
  for (u32 i = 0; i < s->length - 1; i++)
  {
    if (s->buffer[i] == '#' && s->buffer[i + 1] == '#')
    {
      return i;
    }
  }
  return -1;
}

UI_Comm UI::comm_from_widget(UI_Widget* widget)
{
  UI_Comm comm      = {};

  f32     x         = this->input->mouse_position[0];
  f32     y         = this->input->mouse_position[1];

  comm.hovering     = widget->x[0] <= x && x <= widget->x[1] && widget->y[1] <= y && y <= widget->y[0];

  comm.left_clicked = comm.hovering && this->input->is_left_mouse_clicked();
  comm.widget       = widget;

  return comm;
}

UI_Comm UI::UI_Button(const char* string)
{
  UI_Widget* current_layout = this->current_layout;
  UI_Widget* last           = current_layout->last;
  // compute the position of the button and update the current layout
  f32 x[2];
  f32 y[2];
  if (last)
  {
    x[0] = last->x[0];
    x[1] = last->x[1];

    y[0] = last->y[0];
    y[1] = last->y[1];
  }
  UI_WidgetFlags flags = UI_WidgetFlag_DrawText | UI_WidgetFlag_DrawBackground;
  String         s;
  s.length          = strlen(string);
  s.buffer          = (char*)string;
  UI_Widget* parent = current_layout;

  f32        relative[2];
  Color      c              = {};
  UI_Widget* widget         = this->widget_make(x, y, flags, relative, s, parent, last, c);
  widget->prev              = last;
  widget->font_size         = current_layout->font_size;
  widget->text_color        = current_layout->text_color;
  widget->background        = current_layout->background;
  widget->text_alignment[0] = current_layout->text_alignment[0];
  widget->text_alignment[1] = current_layout->text_alignment[1];

  if (last)
  {
    last->next = widget;
  }
  else
  {
    current_layout->last  = widget;
    current_layout->first = widget;
  }
  current_layout->last = widget;

  return this->comm_from_widget(widget);
}

UI_Widget::UI_Widget(f32 x[2], f32 y[2], UI_WidgetFlags flags, String string, UI_Widget* parent, f32 relative[2], Color background)
{
  this->x[0]       = x[0];
  this->x[1]       = x[1];

  this->y[0]       = y[0];
  this->y[1]       = y[1];

  this->flags      = flags;
  this->text       = string;
  this->background = background;
}

UI_Widget* UI::widget_make(f32 x[2], f32 y[2], UI_WidgetFlags flags, f32 relative[2], String string, UI_Widget* parent, UI_Widget* prev, Color background)
{
  UI_Widget* widget  = sta_arena_push_struct(this->arena, UI_Widget);
  widget->parent     = parent;
  widget->flags      = flags;
  widget->x[0]       = x[0];
  widget->x[1]       = x[1];

  widget->y[0]       = y[0];
  widget->y[1]       = y[1];
  widget->background = background;

  if (prev)
  {
    // add relative to x,y
    widget->x[0] += prev->relative_position[0];
    widget->x[1] += prev->relative_position[0];
    widget->y[0] += prev->relative_position[1];
    widget->y[1] += prev->relative_position[1];
  }

  widget->relative_position[0] = relative[0];
  widget->relative_position[1] = relative[1];

  widget->text                 = string;
  widget->prev                 = prev;
  if (prev)
  {
    prev->next = widget;
  }

  return widget;
}

static inline f32 normalize(f32 x, f32 low, f32 high)
{
  return ((x - low) / (high - low)) * 2.0f - 1.0f;
}

void UI::push_layout(f32 x[2], f32 y[2], UI_WidgetFlags flags, f32 relative[2], Color background, Color text_color, TextAlignment text_alignment[2], f32 font_size)
{
  this->push_layout(x, y, flags, relative);
  UI_Widget* layout         = this->current_layout;
  layout->background        = background;
  layout->text_color        = text_color;
  layout->text_alignment[0] = text_alignment[0];
  layout->text_alignment[1] = text_alignment[1];
  layout->font_size         = font_size;
}

void UI::push_layout(f32 x[2], f32 y[2])
{
  f32 relative[2] = {0, 0};
  this->push_layout(x, y, 0, relative);
}
void UI::push_layout(f32 x[2], f32 y[2], UI_WidgetFlags flags, f32 relative[2])
{
  UI_Widget* layout     = this->current_layout;
  UI_Widget* new_layout = sta_arena_push_struct(this->arena, UI_Widget);
  printf("Layout: %ld\n", (u64)new_layout);
  if (layout)
  {
    x[0] = normalize(x[0], layout->x[0], layout->x[1]);
    x[1] = normalize(x[0], layout->x[0], layout->x[1]);
    y[0] = normalize(y[0], layout->y[0], layout->y[1]);
    y[1] = normalize(y[1], layout->y[0], layout->y[1]);
  }

  new_layout->relative_position[0] = relative[0];
  new_layout->relative_position[1] = relative[1];
  new_layout->x[0]                 = x[0];
  new_layout->x[1]                 = x[1];
  new_layout->y[0]                 = y[0];
  new_layout->y[1]                 = y[1];

  if (layout)
  {
    new_layout->parent = layout;
    if (layout->last)
    {
      layout->last->next = new_layout;
    }
    else
    {
      layout->first = new_layout;
    }
    new_layout->prev = layout->last;
    layout->last     = new_layout;
  }
  new_layout->flags    = flags;
  this->current_layout = new_layout;
}
void UI::pop_layout()
{
  this->current_layout = this->current_layout->parent;
}

UI_State ui_build_main_menu(UI* ui)
{

  f32 layout_x[2]        = {-0.3f, 0.3f};
  f32 layout_y[2]        = {-0.8f, 0.8f};
  f32 layout_relative[2] = {0, 0};
  f32 button_relative[2] = {0.0f, -0.25f};
  f32 button_x[2]        = {-1.0f, 1.0f};
  f32 button_y[2]        = {-1.0f, 1.0f};

  ui->push_layout(layout_x, layout_y, 0, layout_relative);
  Color         background   = RED;
  Color         text_color   = WHITE;
  f32           font_size    = 0.1f;
  TextAlignment alignment[2] = {TextAlignment_Centered, TextAlignment_Centered};
  ui->push_layout(button_x, button_y, 0, button_relative, background, text_color, alignment, font_size);

  if (ui->UI_Button("START").left_clicked)
  {
    return UI_STATE_GAME_RUNNING;
  }

  if (ui->UI_Button("OPTIONS").left_clicked)
  {
    return UI_STATE_OPTIONS_MENU;
  }

  if (ui->UI_Button("EXIT").left_clicked)
  {
    return UI_STATE_EXIT_GAME;
  }
  ui->pop_layout();
  ui->pop_layout();

  return UI_STATE_MAIN_MENU;
}
UI_State UI::build(UI_State state)
{
  f32 x[2] = {-1.0f, 1.0f};
  f32 y[2] = {-1.0f, 1.0f};
  this->push_layout(x, y);
  switch (state)
  {
  case UI_STATE_MAIN_MENU:
  {
    return ui_build_main_menu(this);
  }
  case UI_STATE_OPTIONS_MENU:
  case UI_STATE_GAME_RUNNING:
  case UI_STATE_EXIT_GAME:
  {
    return state;
  }
  }
  this->pop_layout();
  return state;
}

#define DrawBackground(flags) (flags & UI_WidgetFlag_DrawBackground) > 0
#define DrawText(flags)       (flags & UI_WidgetFlag_DrawText) > 0

void UI_Widget::render(Renderer* renderer)
{
  printf("(%f %f), (%f, %f)\n", this->x[0], this->x[1], this->y[0], this->y[1]);
  if (DrawBackground(this->flags))
  {
    f32 min[2] = {this->x[0], this->y[0]};
    f32 max[2] = {this->x[1], this->y[1]};
    renderer->render_2d_quad(min, max, this->background);
  }
  if (DrawText(this->flags))
  {
    f32 x = this->x[1] - this->x[0], y = this->y[0] - this->y[1];
    renderer->render_text(this->text.buffer, this->text.length, x, y, this->text_alignment[0], this->text_alignment[1], this->background, this->font_size);
  }

  if (this->next)
  {
    this->next->render(renderer);
  }
  if (this->first)
  {
    this->first->render(renderer);
  }
}

void UI::render(Renderer* renderer)
{

  assert(this->current_layout->parent == 0);
  this->current_layout->render(renderer);
  exit(1);

  this->arena->ptr     = 0;
  this->current_layout = 0;
}
