#include "common.h"
#include "font.h"
#include "input.h"
#include "renderer.h"
#include "ui.h"

int main()
{

  Font font;
  font.parse_ttf("./data/fonts/JetBrainsMono-Bold.ttf");
  const int  screen_width  = 620;
  const int  screen_height = 480;
  Renderer   renderer(screen_width, screen_height, &font, 0);

  f32        min[2]      = {-1.0f, -1.0f};
  f32        max[2]      = {1.0f, 1.0f};

  InputState input_state = {};
  u32        ticks       = 0;
  UI         ui(&renderer, &input_state);

  ui.add_layout(min, max);
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

    renderer.render_text("3", 1, 0.1f, 0, 0, (TextAlignment)0, (TextAlignment)0);

    renderer.swap_buffers();
  }
}
