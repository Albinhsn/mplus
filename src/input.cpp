#include "input.h"
bool InputState::is_key_pressed(u32 code)
{
  for (u32 i = 0; i < this->event_count; i++)
  {
    if (this->events[i].state == EVENT_KEY_DOWN && this->events[i].key == code)
    {
      return true;
    }
  }
  return false;
}

bool InputState::is_key_released(u32 code)
{
  for (u32 i = 0; i < this->event_count; i++)
  {
    if (this->events[i].state == EVENT_KEY_RELEASE && this->events[i].key == code)
    {
      return true;
    }
  }
  return false;
}

bool InputState::is_left_mouse_pressed()
{
  for (u32 i = 0; i < this->event_count; i++)
  {
    if (this->events[i].state == EVENT_MOUSE_DOWN && this->events[i].key == MOUSE_BUTTON_LEFT)
    {
      return true;
    }
  }
  return false;
}
bool InputState::is_left_mouse_released()
{
  for (u32 i = 0; i < this->event_count; i++)
  {
    if (this->events[i].state == EVENT_MOUSE_RELEASE && this->events[i].key == MOUSE_BUTTON_LEFT)
    {
      return true;
    }
  }
  return false;
}

void InputSequence::update(InputEvent* new_events, u32 new_event_count)
{
  u32 currently_matched = __builtin_popcount(this->matched);
  u32 new_event_idx     = 0;
  while (currently_matched < this->event_count && new_event_idx < new_event_count)
  {
    InputEvent new_event = new_events[new_event_idx];
    // only check key events not mouse/quit events
    if (new_event.state != EVENT_KEY_RELEASE)
    {
      new_event_idx++;
      continue;
    }

    InputEvent current_event = this->events[currently_matched];

    if (current_event.key == new_event.key && current_event.state == new_event.state)
    {
      this->matched |= (1 << currently_matched);
      currently_matched++;
    }
    else
    {
      currently_matched = 0;
      this->matched     = 0;
    }
    new_event_idx++;
  }
}
static inline bool is_symmetric_event_release(InputEvent e0, InputEvent e1)
{
  if (e0.state == EVENT_MOUSE_DOWN && e1.state == EVENT_MOUSE_RELEASE && e0.key == e1.key)
  {
    return true;
  }
  return e0.state == EVENT_KEY_DOWN && e1.state == EVENT_KEY_RELEASE && e0.key == e1.key;
}

static inline bool is_persistent(InputEvent event, InputEvent* events, u32 event_count)
{
  if (!(event.state == EVENT_MOUSE_DOWN || event.state == EVENT_KEY_DOWN))
  {
    return false;
  }
  for (u32 i = 0; i < event_count; i++)
  {
    if (is_symmetric_event_release(event, events[i]))
    {
      return false;
    }
  }
  return true;
}

static inline void clear_input(InputState* input)
{
  InputEvent persistent_events[input->event_count];
  u32        count = 0;
  for (u32 i = 0; i < input->event_count; i++)
  {
    if (is_persistent(input->events[i], input->events, input->event_count))
    {
      persistent_events[count++] = input->events[i];
    }
  }
  memset(input->events, 0, sizeof(InputEvent) * input->event_count);
  for (u32 i = 0; i < count; i++)
  {
    input->events[i] = persistent_events[i];
  }
  input->event_count       = count;
  input->mouse_relative[0] = 0;
  input->mouse_relative[1] = 0;
}

static inline void add_event(InputState* input, InputEventType type, u32 key)
{
  if (input->event_count > ArrayCount(input->events))
  {
    for(u32 i = 0; i < ArrayCount(input->events); i++){
      input->events[i].debug();
    }
    assert(input->event_count < ArrayCount(input->events) && "Overflow for input events!");
  }

  input->events[input->event_count].state = type;
  input->events[input->event_count].key   = key;
  input->event_count++;
}
static inline void add_event(InputState* input, InputEventType type)
{
  input->events[input->event_count].state = type;
  input->events[input->event_count].key   = 0;
  input->event_count++;
}
static inline u32 convert_sdl_to_mouse_key(u8 button)
{
  switch (button)
  {
  case SDL_BUTTON_LEFT:
  {
    return MOUSE_BUTTON_LEFT;
  }
  case SDL_BUTTON_RIGHT:
  {
    return MOUSE_BUTTON_RIGHT;
  }
  case SDL_BUTTON_MIDDLE:
  {
    return MOUSE_BUTTON_MIDDLE;
  }
  }
  return -1;
}

void InputState::update()
{
  // clear from previous frame
  clear_input(this);

  // actually poll more events
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    ImGui_ImplSDL2_ProcessEvent(&event);
    switch (event.type)
    {
    case SDL_QUIT:
    {
      add_event(this, EVENT_QUIT);
      break;
    }
    case SDL_MOUSEMOTION:
    {
      i32 w, h;
      SDL_GetWindowSize(this->window, &w, &h);
      this->mouse_position[0] = (event.motion.x / (f32)w) * 2.0f - 1.0f;
      this->mouse_position[1] = -((event.motion.y / (f32)h) * 2.0f - 1.0f);
      this->mouse_relative[0] = event.motion.xrel;
      this->mouse_relative[1] = event.motion.yrel;
      break;
    }
    case SDL_MOUSEBUTTONUP:
    {
      i32 mouse_button = convert_sdl_to_mouse_key(event.button.button);
      if (mouse_button != -1)
      {
        add_event(this, EVENT_MOUSE_RELEASE, mouse_button);
      }
      break;
    }
    case SDL_MOUSEBUTTONDOWN:
    {
      i32 mouse_button = convert_sdl_to_mouse_key(event.button.button);
      if (mouse_button != -1)
      {
        add_event(this, EVENT_MOUSE_DOWN, mouse_button);
      }
      break;
    }
    case SDL_KEYUP:
    {
      add_event(this, EVENT_KEY_RELEASE, event.key.keysym.sym);
      break;
    }
    case SDL_KEYDOWN:
    {
      add_event(this, EVENT_KEY_DOWN, event.key.keysym.sym);
      break;
    }
    default:
    {
      // ToDo debug log and check what it is?
      break;
    }
    }
  }

  // update sequences if applicable
  for (u32 i = 0; i < this->sequence_count; i++)
  {
    this->sequences[i].update(this->events, this->event_count);
  }
}

u32 InputState::add_sequence(InputEvent* events, u32 event_count)
{
  if (event_count > 32)
  {
    assert(0 && "Can't create a sequence of more then 32 events!");
  }

  RESIZE_ARRAY(this->sequences, InputSequence, this->sequence_count, this->sequence_capacity);

  InputSequence* sequence = &this->sequences[this->sequence_count];
  sequence->events        = events;
  sequence->event_count   = event_count;
  sequence->matched       = 0;

  u32 sequence_index      = this->sequence_count;
  this->sequence_count++;
  return sequence_index;
}
bool InputState::check_sequence(u32 id)
{
  InputSequence* sequence = &this->sequences[id];
  return sequence->event_count == __builtin_popcount(sequence->matched);
}
bool InputState::should_quit()
{
  for (u32 i = 0; i < this->event_count; i++)
  {
    InputEvent event = this->events[i];
    if ((event.state == EVENT_KEY_RELEASE && event.key == ASCII_ESCAPE) || event.state == EVENT_QUIT)
    {
      return true;
    }
  }
  return false;
}
