#include "ui.h"
#include "common.h"

void UI::add_layout(f32 min[2], f32 max[2])
{
  RESIZE_ARRAY(this->layouts, UI_Layout, this->layout_count, this->layout_cap);

  // add to our current min/max
  this->layouts[this->layout_count++] = UI_Layout(min, max);
}

UI_Comm UI::comm_from_widget(UI_Widget* widget)
{
  UI_Comm comm  = {};

  f32     x     = (this->input->mouse_position[0] / (f32)this->renderer->screen_width) * 2.0f - 1.0f;
  f32     y     = (this->input->mouse_position[1] / (f32)this->renderer->screen_height) * 2.0f - 1.0f;

  comm.hovering = widget->x[0] <= x && x <= widget->x[1] && widget->y[1] <= y && y <= widget->y[0];

  comm.left_clicked = comm.hovering && this->input->is_left_mouse_clicked();
  comm.widget       = widget;

  return comm;
}

UI_Layout UI::current_layout()
{
  assert(this->layout_count > 0 && "No layout!!!");
  return this->layouts[this->layout_count - 1];
}

UI_Comm UI::UI_Button(String string, f32 min[2], f32 max[2])
{
  UI_Widget widget(min, max, UI_WidgetFlag_DrawBackground, string, WHITE);
  this->renderer->render_2d_quad(min, max, widget.background);
  return this->comm_from_widget(&widget);
}
