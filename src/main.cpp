#include "common.h"
#include "files.h"
#include "renderer.h"
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#define IMGUI_DEFINE_MATH_OPERATORS

#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>

#include "common.cpp"
#include "platform.h"
#include "sdl.cpp"
#include "vector.cpp"
#include "shader.cpp"
#include "collision.cpp"

#include "../libs/imgui/backends/imgui_impl_opengl3.cpp"
#include "../libs/imgui/backends/imgui_impl_sdl2.cpp"
#include "../libs/imgui/imgui.cpp"
#include "../libs/imgui/imgui_draw.cpp"
#include "../libs/imgui/imgui_tables.cpp"
#include "../libs/imgui/imgui_widgets.cpp"

#include "files.cpp"
#include "font.cpp"
#include "input.cpp"
#include "renderer.cpp"
#include "ui.cpp"
#include "gltf.cpp"

struct Camera
{
  Vector3 translation;
  Vector3 rotation;
};

int main()
{

  AFont font;
  font.parse_ttf("./data/fonts/OpenSans-Regular.ttf");
  const int  screen_width  = 620;
  const int  screen_height = 480;
  Renderer   renderer(screen_width, screen_height, &font, 0);

  InputState input_state(renderer.window);
  u32        ticks = 0;
  Arena      arena(1024 * 1024);

  init_imgui(renderer.window, renderer.context);
  // create a the first map on the xz plane
  //   render some random texture for debug purposes
  // be able to move around the camera with wasd and zoom
  ModelData map = {};
  sta_parse_wavefront_object_from_file(&map, "./data/map_with_hole.obj");
  Shader model_shader("./shaders/model.vert", "./shaders/model.frag");
  Mat44  ident = {};
  ident.identity();
  model_shader.use();
  model_shader.set_mat4("view", ident);
  model_shader.set_mat4("projection", ident);
  BufferAttributes map_attributes[3] = {
      {3, GL_FLOAT},
      {2, GL_FLOAT},
      {3, GL_FLOAT}
  };
  TargaImage image     = {};

  sta_targa_read_from_file_rgba(&image, "./data/blizzard.tga");
  u32 map_buffer = renderer.create_buffer_indices(sizeof(VertexData) * map.vertex_count, map.vertices, map.vertex_count, map.indices, map_attributes, ArrayCount(map_attributes));
  u32 texture    = renderer.create_texture(image.width, image.height, image.data);
  renderer.bind_texture(texture, 0);
  // AnimationModel model = {};
  // gltf_parse(&model, "./data/first_sphere.glb");

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
    // ImGui_ImplOpenGL3_NewFrame();
    // ImGui_ImplSDL2_NewFrame();
    // ImGui::NewFrame();

    renderer.clear_framebuffer();
    renderer.render_buffer(map_buffer);

    // ImGui::Render();
    // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    renderer.swap_buffers();
  }
}
