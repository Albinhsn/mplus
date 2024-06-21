#ifndef INPUT_H
#define INPUT_H

#include "common.h"
#include "platform.h"
#define ASCII_ESCAPE    27
#define ASCII_RETURN    13
#define ASCII_SPACE     32
#define ASCII_BACKSPACE 8

enum MouseButtonType
{
  MOUSE_BUTTON_LEFT,
  MOUSE_BUTTON_RIGHT,
  MOUSE_BUTTON_MIDDLE,
};

enum InputEventType
{
  EVENT_KEY_DOWN,
  EVENT_KEY_RELEASE,
  EVENT_MOUSE_DOWN,
  EVENT_MOUSE_RELEASE,
  EVENT_QUIT
};

struct InputEvent
{
  InputEventType state;
  u32            key;
};
struct InputSequence
{
public:
  void        update(InputEvent* new_events, u32 new_event_count);
  InputEvent* events;
  u32         event_count;
  u32         matched;
};

struct InputState
{
public:
  InputState()
  {
    this->sequence_count    = 0;
    this->sequence_capacity = 1;
    this->sequences         = (InputSequence*)sta_allocate_struct(InputSequence, 1);
  }
  InputSequence* sequences;
  i32            mouse_position[2];
  i32            mouse_relative[2];
  u32            sequence_count;
  u32            sequence_capacity;
  InputEvent     events[25];
  u32            event_count;

  void           update();
  u32            add_sequence(InputEvent* events, u32 event_count);
  bool           check_sequence(u32 id);
  bool           should_quit();
  bool           is_left_mouse_clicked();

private:
};

#endif
