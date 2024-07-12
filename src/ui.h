#ifndef UI_H
#define UI_H
#include "common.h"
#include "sdl.h"

void init_imgui(SDL_Window* window, SDL_GLContext context);

enum UI_State
{
  UI_STATE_MAIN_MENU,
  UI_STATE_GAME_RUNNING,
  UI_STATE_OPTIONS_MENU,
};
UI_State render_ui(UI_State state, Hero* player, u32 ms, f32 fps, u32 update_ticks, u32 render_ticks, u32 game_running_ticks, u32 screen_height, u32 clear_tick, u32 render_ui_tick);
void render_ui_frame();
void init_new_ui_frame();

#endif
