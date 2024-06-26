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

  init_imgui(renderer.window, renderer.context);

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
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();

    renderer.clear_framebuffer();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    renderer.swap_buffers();
  }
}
