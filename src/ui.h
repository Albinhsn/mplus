#ifndef UI_H
#define UI_H
#include "common.h"

// core
//  common codepaths and helpers
// builder code
//  in charge of arranging instances of widgets

// structure of a frame
//  buld widget hierarchy
//  autolayout pass
//  render
// hot_t and active_t

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

struct UI_Widget
{
  UI_Widget * first;
  UI_Widget * last;
  UI_Widget * next;
  UI_Widget * prev;
  UI_Widget * parent;

  UI_Widget * hash_next;
  UI_Widget * hash_prev;

  UI_Key key;
  u64 last_frame_touched_index;

  UI_Size semantic_size[Axis2_COUNT];
  f32     computed_relative_position[Axi2_COUNT];
  f32     rect[2][2];
};

struct UI_Button
{
};

struct UI_Checkbox
{
};
struct UI_RadioButton
{
};
struct UI_Slider
{
};
struct UI_TextField
{
};

struct UI_ScrollBar
{
};

#endif
