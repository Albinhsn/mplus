#include "common.h"
#include "font.h"
#include "input.h"
#include "renderer.h"
#include "ui.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <iterator>

void update_ui_state(UI_State new_ui_state, UI_State& ui_state, UI_State& prev_ui_state)
{
  if (new_ui_state != ui_state)
  {
    if (new_ui_state == UI_STATE_RETURN && ui_state == UI_STATE_OPTIONS_MENU)
    {
      ui_state = prev_ui_state;
    }
    else
    {
      prev_ui_state = ui_state;
      ui_state      = new_ui_state;
    }
  }
}
int main()
{

  Font font;
  font.parse_ttf("./data/fonts/OpenSans-Regular.ttf");
  const int  screen_width  = 620;
  const int  screen_height = 480;
  Renderer   renderer(screen_width, screen_height, &font, 0);

  f32        min[2] = {-1.0f, -1.0f};
  f32        max[2] = {1.0f, 1.0f};

  InputState input_state(renderer.window);
  u32        ticks = 0;
  Arena      arena(1024 * 1024);
  UI         ui(&input_state, &arena, &renderer);

  // run_debug(font, renderer, ticks, input_state);
  // return 0;

  enum UI_State ui_state = UI_STATE_MAIN_MENU;
  enum UI_State prev_ui_state;

  while (true)
  {
    input_state.update();
    if (input_state.should_quit() || ui_state == UI_STATE_EXIT_GAME)
    {
      break;
    }

    if (ticks + 16 < SDL_GetTicks())
    {
      ticks = SDL_GetTicks() + 16;
    }
    renderer.clear_framebuffer();
    update_ui_state(ui.build(ui_state, ticks), ui_state, prev_ui_state);
    ui.render();

    renderer.swap_buffers();
  }
}
