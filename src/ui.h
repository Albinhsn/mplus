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
  UI_STATE_OPTIONS_MENU,
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
  void render(Renderer* renderer);
  UI_Widget(f32 x[2], f32 y[2], UI_WidgetFlags flags, String string, UI_Widget* parent, f32 relative[2], Color background);

  UI_Widget* parent;
  UI_Widget* next;
  UI_Widget* prev;
  UI_Widget* first;
  UI_Widget* last;

  UI_Key     key;
  u64        last_frame_touched_index;

  // build info
  UI_WidgetFlags flags;
  String         text;
  Color          text_color;
  TextAlignment  text_alignment[2];
  f32            font_size;
  Color          background;

  //  the relative position from the bounding box that any item is placed
  f32 relative_position[2];

  //  bounding box of the widget/layout
  f32 x[2];
  f32 y[2];

  f32 hot_t;    // about to be interacted with
  f32 active_t; // interacted with
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

struct UI_Persistent_Data
{
  f32 hot_t;
  f32 active_t;
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
  void                add(UI_Key key, UI_Persistent_Data data);
};

struct UI
{
public:
  UI(InputState* input, Arena * arena)
  {
    this->input = input;
    this->arena = arena;
  }

  UI_Comm    comm_from_widget(UI_Widget* widget);
  UI_Comm    UI_Button(const char* string);
  void       get_persistent_data(UI_Widget* widget);
  UI_Widget* widget_make(f32 x[2], f32 y[2], UI_WidgetFlags flags, f32 relative[2], String string, UI_Widget* parent, UI_Widget* prev, Color background);
  UI_State   build(UI_State state);
  void       render(Renderer* renderer);
  void       push_layout(f32 x[2], f32 y[2], UI_WidgetFlags flags, f32 relative[2]);
  void       push_layout(f32 x[2], f32 y[2]);
  void push_layout(f32 x[2], f32 y[2], UI_WidgetFlags flags, f32 relative[2], Color background, Color text_color, TextAlignment text_alignment[2], f32 font_size);
  void       pop_layout();
  UI_Widget* current_layout;

private:
  InputState*                    input;
  UI_Widget_Persitent_Data_Table persistent_data;
  Arena*                         arena;
};

UI_State ui_build_main_menu(UI* ui);

#endif
