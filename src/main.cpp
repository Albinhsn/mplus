#include "common.h"
#include "font.h"
#include "input.h"
#include "renderer.h"
#include "ui.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>

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
  UI         ui(&input_state, &arena);

  // run_debug(font, renderer, ticks, input_state);
  // return 0;

  enum UI_State ui_state = UI_STATE_MAIN_MENU;

  while (true)
  {
    input_state.update();
    if (input_state.should_quit())
    {
      break;
    }

    if (ticks + 16 < SDL_GetTicks())
    {
      ticks = SDL_GetTicks() + 16;
    }
    renderer.clear_framebuffer();
    ui.build(ui_state);
    ui.render(&renderer);


    // // font size, wrap
    // const char* s = "abcdefghklmnopqrstuvxyz";
    // renderer.render_text(s, strlen(s), -0.85f, 0.0f, (TextAlignment)0, (TextAlignment)0, WHITE, 0.1f);

    // const char* s2 = "ABCDEFGHIJKLMNOPQRSTUVXYZ";
    // renderer.render_text(s2, strlen(s2), -0.85f, 0.15f, (TextAlignment)0, (TextAlignment)0, WHITE, 0.1f);

    // const char* s3 = "0123456789";
    // renderer.render_text(s3, strlen(s3), -0.85f, -0.15f, (TextAlignment)0, (TextAlignment)0, WHITE, 0.1f);

    renderer.swap_buffers();
  }
}
