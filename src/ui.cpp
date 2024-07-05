#include "ui.h"
#include "../libs/imgui/imgui.h"
#include "../libs/imgui/backends/imgui_impl_sdl2.h"
#include "../libs/imgui/backends/imgui_impl_opengl3.h"

void init_imgui(SDL_Window* window, SDL_GLContext context)
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  ImGui_ImplSDL2_InitForOpenGL(window, context);
  ImGui_ImplOpenGL3_Init();
}

void render_game_running_ui(Hero* player, u32 ms, f32 fps, u32 update_ticks, u32 render_ticks, u32 game_running_ticks, u32 screen_height)
{

  ImGui::Begin("Frame times");
  ImGui::Text("Update:     %d", update_ticks);
  ImGui::Text("Rendering : %d", render_ticks);
  ImGui::Text("MS: %d", ms);
  ImGui::Text("FPS: %f", fps * 1000);
  ImGui::End();

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowSize(ImVec2(center.x, center.y));

  ImGui::Begin("Game ui!", 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);
  f32 old_size = ImGui::GetFont()->Scale;
  ImGui::GetFont()->Scale *= 2;
  ImGui::PushFont(ImGui::GetFont());

  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
  ImGui::Text("TIMER: %.2f", game_running_ticks / 1000.0f);
  ImGui::Text("HP: %d ", entities[player->entity].hp);
  ImGui::Text("Score:%d", (i32)score);
  ImGui::PopStyleColor();

  ImGui::GetFont()->Scale = old_size;
  ImGui::PopFont();
  ImGui::End();

  u64 width       = 40;
  u64 height      = 40;
  u64 total_width = ArrayCount(player->abilities) * width;
  ImGui::SetNextWindowPos(ImVec2(center.x - (u64)(total_width / 2), screen_height - height), ImGuiCond_Appearing);
  // ImGui::SetNextWindowSize(ImVec2(center.x, center.y));
  ImGui::Begin("Abilities", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
  for (u32 i = 0; i < ArrayCount(player->abilities); i++)
  {
    char buf[64];
    memset(buf, 0, ArrayCount(buf));
    snprintf(buf, ArrayCount(buf), "%.1f", MAX(0, ((i32)player->abilities[i].cooldown - (i32)game_running_ticks) / 1000.0f));

    ImGui::Button(buf, ImVec2(width, height));
    ImGui::SameLine();
  }
  ImGui::End();
}

UI_State render_ui(UI_State state, Hero* player, u32 ms, f32 fps, u32 update_ticks, u32 render_ticks, u32 game_running_ticks, u32 screen_height)
{
  init_new_ui_frame();
  switch (state)
  {
  case UI_STATE_GAME_RUNNING:
  {
    render_game_running_ui(player, ms, fps, update_ticks, render_ticks, game_running_ticks, screen_height);
    return UI_STATE_GAME_RUNNING;
  }
  case UI_STATE_MAIN_MENU:
  {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(ImVec2(center.x, center.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGui::Begin("Main menu!", 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);

    ImGui::Text("MPLUS!");
    if (ImGui::Button("Run game!"))
    {
      ImGui::End();
      return UI_STATE_GAME_RUNNING;
    }

    ImGui::End();
    return UI_STATE_MAIN_MENU;
  }
  case UI_STATE_OPTIONS_MENU:
  {

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(ImVec2(center.x, center.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGui::Begin("Main menu!", 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);

    if (ImGui::Button("Return!"))
    {
      return UI_STATE_GAME_RUNNING;
    }

    ImGui::End();
    return UI_STATE_OPTIONS_MENU;
  }
  }
  return state;
}

void init_new_ui_frame()
{
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
}
void render_ui_frame()
{
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
