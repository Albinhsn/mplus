#include "animation.h"
#include "common.h"
#include "files.h"
#include "renderer.h"
#include <cmath>
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
#include "animation.cpp"
#include "input.cpp"
#include "renderer.cpp"
#include "ui.cpp"
#include "gltf.cpp"

struct Camera
{
  Vector3 translation;
};

struct Character
{
  AnimationModel* model;
  Vector2         position;
  i32             animation_tick_start;
};

void handle_movement(Character* character, Shader* char_shader, InputState* input, u32 tick)
{
  Vector2 velocity = {};
  f32     MS       = 0.01f;
  if (input->is_key_pressed('w'))
  {
    velocity.y += MS;
  }
  if (input->is_key_pressed('a'))
  {
    velocity.x -= MS;
  }
  if (input->is_key_pressed('s'))
  {
    velocity.y -= MS;
  }
  if (input->is_key_pressed('d'))
  {
    velocity.x += MS;
  }
  Vector2 v         = character->position;

  f32*    mouse_pos = input->mouse_position;
  f32     angle     = atan2f(mouse_pos[1] - v.y, mouse_pos[0] - v.x) - DEGREES_TO_RADIANS(90);
  // the velocity should be based on the direction as well

  character->position.x = cosf(angle) * velocity.x - sinf(angle) * velocity.y + v.x;
  character->position.y = cosf(angle) * velocity.y + sinf(angle) * velocity.x + v.y;

  Mat44 a               = {};
  a.identity();
  a = a.scale(0.05f).rotate_x(90.0f).rotate_z(RADIANS_TO_DEGREES(angle)).translate(Vector3(character->position.x, character->position.y, 0.0));
  char_shader->use();
  char_shader->set_mat4("view", a);

  if (velocity.x != 0 || velocity.y != 0)
  {
    if (character->animation_tick_start == -1)
    {
      character->animation_tick_start = tick;
    }
    tick -= character->animation_tick_start;
    update_animation(*character->model, *char_shader, tick);
  }
  else
  {
    Mat44 joint_transforms[character->model->skeleton.joint_count];
    for (int i = 0; i < character->model->skeleton.joint_count; i++)
    {
      joint_transforms[i].identity();
    }
    char_shader->set_mat4("jointTransforms", joint_transforms, character->model->skeleton.joint_count);
    character->animation_tick_start = -1;
  }
}

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
  Shader map_shader("./shaders/model.vert", "./shaders/model.frag");
  Mat44  ident = {};
  ident.identity();
  map_shader.use();
  map_shader.set_mat4("view", ident);
  map_shader.set_mat4("projection", ident);
  BufferAttributes map_attributes[3] = {
      {3, GL_FLOAT},
      {2, GL_FLOAT},
      {3, GL_FLOAT}
  };
  TargaImage image = {};
  // renderer.toggle_wireframe_on();

  sta_targa_read_from_file_rgba(&image, "./data/blizzard.tga");
  u32            map_buffer = renderer.create_buffer_indices(sizeof(VertexData) * map.vertex_count, map.vertices, map.vertex_count, map.indices, map_attributes, ArrayCount(map_attributes));
  u32            texture    = renderer.create_texture(image.width, image.height, image.data);
  AnimationModel model      = {};
  gltf_parse(&model, "./data/model.glb");
  BufferAttributes animated_attributes[5] = {
      {3, GL_FLOAT},
      {2, GL_FLOAT},
      {3, GL_FLOAT},
      {4, GL_FLOAT},
      {4,   GL_INT}
  };
  u32 character_buffer =
      renderer.create_buffer_indices(sizeof(SkinnedVertex) * model.vertex_count, model.vertices, model.index_count, model.indices, animated_attributes, ArrayCount(animated_attributes));
  Shader char_shader("./shaders/animation.vert", "./shaders/animation.frag");
  char_shader.use();
  Mat44 char_matrices[50];
  for (u32 i = 0; i < ArrayCount(char_matrices); i++)
  {
    char_matrices[i].identity();
  }
  Vector2   char_pos(0.5, 0.5);
  Character character            = {};
  character.model                = &model;
  character.position             = char_pos;
  character.animation_tick_start = 0;

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
      handle_movement(&character, &char_shader, &input_state, ticks);
    }
    // ImGui_ImplOpenGL3_NewFrame();
    // ImGui_ImplSDL2_NewFrame();
    // ImGui::NewFrame();

    renderer.clear_framebuffer();
    // render map
    renderer.bind_texture(texture, 0);
    map_shader.use();
    renderer.render_buffer(map_buffer);
    // render character

    char_shader.use();
    renderer.render_buffer(character_buffer);
    // ImGui::Render();
    // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    renderer.swap_buffers();
  }
}
