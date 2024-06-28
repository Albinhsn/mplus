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
  // create a big quad on the xz plane
  //   render some random texture for debug purposes
  // create a camera position which looks at it
  //  be able to move around it with wasd and zoom

  f32 plane[12] = {
      -0.8, 0, 0.8,  //
      -0.8, 0, -0.8, //
      0.8,  0, -0.8, //
      0.8,  0, 0.8,  //
  };
  // f32 plane[12] = {
  //     -0.8, 0.8,  0, //
  //     -0.8, -0.8, 0, //
  //     0.8,  -0.8, 0, //
  //     0.8,  0.8,  0, //
  // };
  u32 plane_indices[6] = {
      0, 1, 2, 0, 2, 3 //
  };
  Shader           shader("./shaders/plane.vert", "./shaders/plane.frag");
  BufferAttributes attributes[1] = {
      {3, GL_FLOAT}
  };

  shader.use();
  u32   buffer = renderer.create_buffer_indices(sizeof(f32) * ArrayCount(plane), plane, ArrayCount(plane_indices), plane_indices, attributes, ArrayCount(attributes));
  Color white  = WHITE;
  shader.set_float4f("color", (float*)&white);
  renderer.camera.identity();
  renderer.look_at(Vector3(0, -1.5, 0), Vector3(0, 1, 0), Vector3(0, 1, 0), Vector3(0, -1.5, 0));
  // renderer.camera = renderer.camera.rotate_x(90.0f);
  renderer.camera.debug();

  shader.set_mat4("camera", renderer.camera);

  while (true)
  {
    input_state.update();
    if (input_state.should_quit())
    {
      break;
    }

    if (ticks + 16 < SDL_GetTicks())
    {
      ticks           = SDL_GetTicks() + 16;
      // renderer.camera = renderer.camera.rotate_x(1.0f);
      // shader.set_mat4("camera", renderer.camera);
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    renderer.clear_framebuffer();
    renderer.render_buffer(buffer);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    renderer.swap_buffers();
  }
}
