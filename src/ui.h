#ifndef UI_H
#define UI_H
#include "common.h"
#include "input.h"
#include "platform.h"
#include "renderer.h"
#include "vector.h"

// console
// main menu
// settings menu
// debug view (fps, counters etc)

// button
// scrollable view thingy
// slider
// dropdown
// animations, both for scroll and hover etc

enum UI_State
{
  UI_STATE_MAIN_MENU,
  UI_STATE_GAME_RUNNING,
  UI_STATE_CONSOLE,
  UI_STATE_OPTIONS_MENU,
  UI_STATE_RETURN,
  UI_STATE_EXIT_GAME
};

typedef i32 UI_Key;

inline bool UI_Key_Compare(UI_Key a, UI_Key b);
UI_Key      generate_key(String* s);

typedef u32 UI_WidgetFlags;
enum
{
  UI_WidgetFlag_DrawText        = (1 << 1),
  UI_WidgetFlag_DrawBackground  = (1 << 2),
  UI_WidgetFlag_HotAnimation    = (1 << 3),
  UI_WidgetFlag_ActiveAnimation = (1 << 4),
};

struct UI_Widget
{
public:
  void       render(Renderer* renderer);

  UI_Widget* next;
  UI_Key     key;

  // build info
  UI_WidgetFlags flags;
  String         text;
  Color          text_color;
  TextAlignment  text_alignment[2];
  f32            font_size;
  Color          background;

  //  bounding box of the widget/layout
  f32 x[2];
  f32 y[2];
};

struct UI_Comm
{
  UI_Widget* widget;
  Vector2    mouse;
  Vector2    drag_delta;
  bool       left_clicked;
  bool       pressed;
  bool       released;
  bool       dragging;
  bool       hovering;
};

struct UI_Persistent_Data
{
  u64  last_animated_index;
  u64  last_animated_tick;
  f32  animation_progress;
  bool toggled;
};

struct UI_Widget_Persitent_Data_Table
{
  UI_Persistent_Data* persistent_data;
  UI_Key*             widget_keys;
  u32                 widget_key_count;
  u32                 widget_key_capacity;

public:
  UI_Persistent_Data* get(UI_Key key);
  void                remove(UI_Key key);
  UI_Persistent_Data* add(UI_Key key, UI_Persistent_Data data);
};

struct UI_Button_Data
{
  Color         background;
  Color         text_color;
  f32           font_size;
  TextAlignment alignment[2];
  void (*animation_func)(UI_Widget* widget);
};

struct UI
{
public:
  UI(InputState* input, Arena* arena, Renderer * renderer)
  {
    this->input                               = input;
    this->arena                               = arena;
    this->renderer = renderer;
    this->last_tick                           = 0;
    this->persistent_data.widget_key_capacity = 2;
    this->persistent_data.widget_key_count    = 0;
    this->persistent_data.widget_keys         = (UI_Key*)sta_allocate_struct(UI_Key, this->persistent_data.widget_key_capacity);
    this->persistent_data.persistent_data     = (UI_Persistent_Data*)sta_allocate_struct(UI_Persistent_Data, this->persistent_data.widget_key_capacity);
  }

  UI_Comm                        comm_from_widget(UI_Widget* widget);
  UI_Comm                        UI_Button(f32 x[2], f32 y[2], const char* string, UI_Button_Data button_data);
  UI_Comm                        UI_Dropdown(f32 x[2], f32 y[2], const char* string, UI_Button_Data button_data);
  UI_State                       build(UI_State state, u64 tick);
  void                           render();
  UI_Widget_Persitent_Data_Table persistent_data;
  u64                            last_tick;

  UI_Widget*                     current;
  UI_Widget*                     root;
  Renderer*                      renderer;

private:
  InputState* input;
  Arena*      arena;
};

UI_State ui_build_main_menu(UI* ui);

#endif
