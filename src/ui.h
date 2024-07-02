#ifndef UI_H
#define UI_H
#include "common.h"
#include "sdl.h"
#include "input.h"
#include "platform.h"
#include "renderer.h"
#include "vector.h"

void init_imgui(SDL_Window* window, SDL_GLContext context);

enum UI_State
{
  UI_STATE_MAIN_MENU,
  UI_STATE_GAME_RUNNING,
  UI_STATE_CONSOLE,
  UI_STATE_OPTIONS_MENU,
  UI_STATE_EXIT_GAME
};

#endif
