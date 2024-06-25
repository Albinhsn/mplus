#include "ui.h"
#include "common.h"
#include "renderer.h"

static inline f32 normalize(f32 x, f32 low, f32 high)
{
  x = (x + 1.0f) / 2.0f;
  return low + (high - low) * x;
}

UI_Key UI_Key_Null()
{
  return -1;
}
inline bool UI_Key_Compare(UI_Key a, UI_Key b)
{
  return a == b;
}
UI_Persistent_Data* UI_Widget_Persitent_Data_Table::add(UI_Key key, UI_Persistent_Data data)
{
  UI_Key null_key = UI_Key_Null();
  u32    i        = 0;
  for (; i < this->widget_key_count; i++)
  {
    if (UI_Key_Compare(this->widget_keys[i], null_key))
    {
      this->persistent_data[i] = data;
      this->widget_keys[i]     = key;
      return &this->persistent_data[i];
    }
  }

  RESIZE_ARRAY(this->persistent_data, UI_Persistent_Data, this->widget_key_count, this->widget_key_capacity);
  this->persistent_data[i] = data;
  this->widget_keys[i]     = key;
  this->widget_key_count++;
  return &this->persistent_data[i];
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
UI_Key generate_key(const char* string)
{
  String s((char*)string, strlen(string));
  return sta_hash_string_fnv(&s);
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
  UI_Comm comm  = {};

  f32     x     = this->input->mouse_position[0];
  f32     y     = this->input->mouse_position[1];
  comm.mouse.y  = x;
  comm.mouse.x  = y;

  comm.hovering = widget->x[0] <= x && x <= widget->x[1] && widget->y[1] <= y && y <= widget->y[0];
  printf("(%f, %f), (%f, %f) vs (%f, %f)\n", widget->x[0], widget->x[1], widget->y[0], widget->y[1], x, y);

  comm.pressed  = comm.hovering && this->input->is_left_mouse_pressed();
  comm.released = comm.hovering && this->input->is_left_mouse_released();
  comm.widget   = widget;

  return comm;
}

UI_Comm UI::UI_Button(f32 x[2], f32 y[2], const char* string, UI_Button_Data button_data)
{
  UI_Widget* widget         = sta_arena_push_struct(this->arena, UI_Widget);
  widget->x[0]              = x[0];
  widget->x[1]              = x[1];
  widget->y[0]              = y[0];
  widget->y[1]              = y[1];
  widget->key               = generate_key(string);

  widget->background        = button_data.background;
  widget->text_color        = button_data.text_color;
  widget->font_size         = button_data.font_size;
  widget->text_alignment[0] = button_data.alignment[0];
  widget->text_alignment[1] = button_data.alignment[1];

  widget->flags             = UI_WidgetFlag_DrawText | UI_WidgetFlag_DrawBackground;
  widget->text.buffer       = (char*)string;
  widget->text.length       = strlen(string);

  if (!this->current)
  {
    this->root    = widget;
    this->current = widget;
  }
  else
  {
    this->current->next = widget;
    this->current       = widget;
  }

  return this->comm_from_widget(widget);
}

void animate_main_menu_button(UI_Comm comm, UI* ui, u64 tick)
{

  UI_Widget* btn                  = comm.widget;
  u64        total_animation_time = 1000;
  f32        total_size_x         = 0.1f;
  f32        total_size_y         = 0.05f;
  if (comm.hovering)
  {
    UI_Persistent_Data* persistent_data = ui->persistent_data.get(btn->key);
    if (!persistent_data)
    {
      UI_Persistent_Data data;
      data.last_animated_tick  = tick;
      data.last_animated_index = ui->last_tick;
      data.progress            = 0;
      data.toggled             = false;
      persistent_data          = ui->persistent_data.add(btn->key, data);
    }
    // Did animate last frame so should continue
    if (persistent_data->last_animated_index == ui->last_tick - 1)
    {
      u64 tick_difference = tick - persistent_data->last_animated_tick;
      if (tick_difference != 0)
      {
        persistent_data->progress += (tick_difference / (f32)total_animation_time);
        persistent_data->progress           = MIN(persistent_data->progress, 1.0f);
        persistent_data->last_animated_tick = tick;
      }
      f32 x = persistent_data->progress * total_size_x;
      f32 y = persistent_data->progress * total_size_y;
      btn->x[0] -= x;
      btn->x[1] += x;
      btn->y[0] += y;
      btn->y[1] -= y;
    }
    else
    {
      persistent_data->last_animated_tick = tick;
    }

    persistent_data->last_animated_index = ui->last_tick;
  }
  else
  {
    UI_Persistent_Data* persistent_data = ui->persistent_data.get(btn->key);
    if (persistent_data && persistent_data->progress != 0)
    {
      u64 tick_difference = tick - persistent_data->last_animated_tick;
      if (tick_difference != 0)
      {
        persistent_data->progress -= (tick_difference / (f32)total_animation_time);
        persistent_data->progress           = MAX(persistent_data->progress, 0.0f);
        persistent_data->last_animated_tick = tick;
      }
      f32 x = persistent_data->progress * total_size_x;
      f32 y = persistent_data->progress * total_size_y;
      btn->x[0] -= x;
      btn->x[1] += x;
      btn->y[0] += y;
      btn->y[1] -= y;
    }
  }
  if (comm.pressed)
  {
    btn->background = Color(0.8f, 0.8f, 0.8f, 1.0f);
  }
}

bool widget_is_toggled(UI_Comm comm, UI* ui)
{
  UI_Persistent_Data* persistent_data = ui->persistent_data.get(comm.widget->key);
  if (!persistent_data)
  {
    UI_Persistent_Data data;
    data.progress            = 0;
    data.last_animated_tick  = 0;
    data.last_animated_index = 0;
    data.toggled             = comm.released;
    ui->persistent_data.add(comm.widget->key, data);

    return data.toggled;
  }
  persistent_data->toggled ^= comm.released;
  return persistent_data->toggled;
}

UI_Comm UI::UI_Dropdown(f32 x[2], f32 y[2], const char* string, UI_Button_Data button_data)
{
  return this->UI_Button(x, y, string, button_data);
}

void update_slider(UI_Comm comm)
{
  UI_Widget* widget = comm.widget;
}

UI_State ui_build_options_menu(UI* ui, u64 tick)
{
  f32     slider_x[2]        = {-0.4f, 0.4f};
  f32     slider_y[2]        = {0.8f, 0.7f};
  f32     slider_btn_x[2]    = {-0.05f, 0.05f};
  f32     slider_btn_y[2]    = {-1.0f, 1.0f};

  UI_Comm volume_slider_comm = ui->UI_Slider(slider_x, slider_y, slider_btn_x, slider_btn_y, "volume");
  update_slider(volume_slider_comm);

  UI_Button_Data button_data = {
      WHITE, RED, 0.1f, {TextAlignment_Centered, TextAlignment_Centered}
  };

  f32 dropdown_x[2]       = {-0.2f, 0.2f};
  f32 dropdown_y[2]       = {0.6f, 0.5f};

  f32 dropdown_relative_y = -0.1f;

  // ToDo format string?
  const char* dropdown_item_strings[]    = {"1920x1080", "1600x900", "1280x720"};
  u64         dropdown_item_values[3][2] = {
      {1920, 1080},
      {1600,  900},
      {1280,  720},
  };

  UI_Comm resolution_comm = ui->UI_Dropdown(dropdown_x, dropdown_y, "Resoluton", button_data);

  if (widget_is_toggled(resolution_comm, ui))
  {
    for (u32 i = 0; i < ArrayCount(dropdown_item_strings); i++)
    {
      dropdown_y[0] += dropdown_relative_y;
      dropdown_y[1] += dropdown_relative_y;
      UI_Comm comm = ui->UI_Button(dropdown_x, dropdown_y, dropdown_item_strings[i], button_data);
      if (comm.released)
      {
        ui->renderer->change_screen_size(dropdown_item_values[i][0], dropdown_item_values[i][1]);
      }
    }
  }

  f32     button_x[2] = {-0.3f, 0.3f};
  f32     button_y[2] = {-0.3f, -0.5f};

  UI_Comm return_comm = ui->UI_Button(button_x, button_y, "RETURN", button_data);
  animate_main_menu_button(return_comm, ui, tick);
  if (return_comm.released)
  {
    return UI_STATE_RETURN;
  }

  return UI_STATE_OPTIONS_MENU;
}
UI_State ui_build_main_menu(UI* ui, u64 tick)
{

  f32            button_x[2] = {-0.3f, 0.3f};
  f32            button_y[2] = {0.4f, 0.2f};
  f32            relative_y  = -0.3f;

  UI_Button_Data button_data = {
      WHITE, RED, 0.1f, {TextAlignment_Centered, TextAlignment_Centered}
  };

  UI_Comm start_comm = ui->UI_Button(button_x, button_y, "START", button_data);
  animate_main_menu_button(start_comm, ui, tick);
  if (start_comm.released)
  {
    printf("Start!\n");
    return UI_STATE_GAME_RUNNING;
  }

  button_y[0] += relative_y;
  button_y[1] += relative_y;

  UI_Comm options_comm = ui->UI_Button(button_x, button_y, "OPTIONS", button_data);
  animate_main_menu_button(options_comm, ui, tick);
  if (options_comm.released)
  {
    printf("Options!\n");
    return UI_STATE_OPTIONS_MENU;
  }
  button_y[0] += relative_y;
  button_y[1] += relative_y;

  UI_Comm exit_comm = ui->UI_Button(button_x, button_y, "EXIT", button_data);
  animate_main_menu_button(exit_comm, ui, tick);
  if (exit_comm.released)
  {
    printf("Exiting!\n");
    return UI_STATE_EXIT_GAME;
  }

  return UI_STATE_MAIN_MENU;
}
UI_State UI::build(UI_State state, u64 tick)
{
  this->last_tick++;
  switch (state)
  {
  case UI_STATE_MAIN_MENU:
  {
    return ui_build_main_menu(this, tick);
  }
  case UI_STATE_OPTIONS_MENU:
  {
    return ui_build_options_menu(this, tick);
  }
  case UI_STATE_GAME_RUNNING:
  case UI_STATE_RETURN:
  case UI_STATE_CONSOLE:

  case UI_STATE_EXIT_GAME:
  {
    return state;
  }
  }
  return state;
}

#define DrawBackground(flags) (flags & UI_WidgetFlag_DrawBackground) > 0
#define DrawText(flags)       (flags & UI_WidgetFlag_DrawText) > 0

void UI_Widget::render(Renderer* renderer)
{
  if (DrawBackground(this->flags))
  {
    f32 min[2] = {this->x[0], this->y[0]};
    f32 max[2] = {this->x[1], this->y[1]};
    renderer->render_2d_quad(min, max, this->background);
  }
  if (DrawText(this->flags))
  {
    f32 x = this->x[0] + (this->x[1] - this->x[0]) / 2, y = this->y[0] - (this->y[0] - this->y[1]) / 2;
    renderer->render_text(this->text.buffer, this->text.length, x, y, this->text_alignment[0], this->text_alignment[1], this->text_color, this->font_size);
  }

  if (this->next)
  {
    this->next->render(renderer);
  }
}

UI_Comm UI::UI_Slider(f32 x[2], f32 y[2], f32 btn_x[2], f32 btn_y[2], const char* string)
{
  UI_Widget* widget = sta_arena_push_struct(this->arena, UI_Widget);
  widget->key       = generate_key(string);
  if (!this->persistent_data.get(widget->key))
  {
    UI_Persistent_Data data;
    data.toggled             = false;
    data.last_animated_index = 0;
    data.last_animated_tick  = 0;
    data.progress            = 0.5f;
    this->persistent_data.add(widget->key, data);
  }

  widget->x[0] = x[0];
  widget->x[1] = x[1];
  widget->y[0] = y[0];
  widget->y[1] = y[1];

  return comm_from_widget(widget);
}

void UI::render()
{

  if (this->root)
  {
    this->renderer->enable_2d_rendering();
    this->root->render(renderer);
    this->renderer->disable_2d_rendering();
  }

  this->arena->ptr = 0;
  this->root       = 0;
  this->current    = 0;
}
