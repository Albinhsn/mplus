#include "animation.h"
#include "common.h"
#include "files.h"
#include "font.h"
#include "gltf.h"
#include "input.h"
#include "platform.h"
#include "sdl.h"
#include "shader.h"
// #include "ui.h"
#include "vector.h"
#include <cassert>
#include <cfloat>
#include <cstdlib>

struct Triangle
{
  Vector2 points[3];
};

inline float orient_2d(Vector2 v0, Vector2 v1, Vector2 p)
{
  return (v0.x - p.x) * (v1.y - p.y) - (v0.y - p.y) * (v1.x - p.x);
}

// assumes that it is a simple polygon!
void triangulate_simple_via_ear_clipping(Triangle** out, Vector2* v_points, u32 point_count)
{
  struct PointDLL
  {
    PointDLL* prev;
    PointDLL* next;
    Vector2   point;
    u32       idx;
  };
  PointDLL* points = (PointDLL*)sta_allocate_struct(PointDLL, point_count);

  // constructs a doubly linked list of the points
  for (i32 i = 0; i < point_count; i++)
  {
    points[i].point = v_points[i];
    points[i].next  = &points[(i + 1) % point_count];
    points[i].prev  = &points[i - 1 < 0 ? point_count - 1 : i - 1];
    points[i].idx   = i;
  }

  u32 ears[point_count];
  u32 ear_count = 0;
  // find the ears
  for (u32 i = 0; i < point_count; i++)
  {
    // get the triangle made up of the 3 vertices
    PointDLL* p1   = &points[i];
    PointDLL* p0   = p1->prev;
    PointDLL* p2   = p1->next;

    PointDLL* curr = p2->next;

    // wierd if it's only three points
    bool found_contained_point = false;
    while (curr != p0)
    {
      float d0, d1, d2;
      bool  neg, pos;

      d0 = orient_2d(p0->point, p1->point, curr->point);
      d1 = orient_2d(p1->point, p2->point, curr->point);
      d2 = orient_2d(p2->point, p0->point, curr->point);

      // check both ways since we don't know the order :)
      neg = (d0 < 0) && (d1 < 0) && (d2 < 0);
      pos = (d0 > 0) && (d1 > 0) && (d2 > 0);
      if (neg || pos)
      {
        found_contained_point = true;
        break;
      }

      curr = curr->next;
    }
    if (!found_contained_point)
    {
      ears[ear_count++] = i;
    }
  }

  for (u32 i = 0; i < ear_count; i++)
  {
    printf("Ear: ");
    points[ears[i]].point.debug();
  }
}

struct TextData
{
  Vector2*  points;
  Triangle* on_curve_points;
  u32*      contours;
  u32       on_curve_point_count;
  u32       contour_count;
  u32       point_count;
};

void get_text_data(Font* font, TextData* text_data, u32* text, u32 text_count)
{
  Vector2* on_curve_points;
  u32      on_curve_point_count;
  for (u32 i = 0; i < text_count; i++)
  {
    Glyph glyph = font->get_glyph(text[i]);
  }

  triangulate_simple_via_ear_clipping(&text_data->on_curve_points, on_curve_points, on_curve_point_count);
}

void calculate_new_pose(AnimationModel anim, Mat44* poses, u32 count, Animation animation, u32 ticks)
{
  u64   loop_time = (u64)(1000 * animation.duration);
  float time      = (ticks % loop_time) / (f32)loop_time;
  if(time == 0){
    time += 0.001f;
  }

  for (u32 i = 0; i < animation.joint_count; i++)
  {
    if (time >= animation.duration - 0.001f)
    {
      time = animation.duration;
    }
    u32        pose_idx = 0;
    JointPose* pose     = &animation.poses[i];
    for (; pose_idx < pose->step_count && pose->steps[pose_idx] < time; pose_idx++)
      ;

    float time_between_poses = (time - pose->steps[pose_idx - 1]) / (pose->steps[pose_idx] - pose->steps[pose_idx - 1]);

    Mat44 first_transform    = pose->local_transform[pose_idx - 1];
    Mat44 second_transform   = pose->local_transform[pose_idx];
    poses[i]                 = interpolate_transforms(first_transform, second_transform, time_between_poses);
  }
}
void update_animation(AnimationModel animation, Shader shader, u32 ticks)
{
  Skeleton* skeleton = &animation.skeleton;
  Mat44     transforms[skeleton->joint_count];
  Mat44     current_poses[skeleton->joint_count];
  calculate_new_pose(animation, current_poses, skeleton->joint_count, animation.animations, ticks);

  Mat44 parent_transforms[skeleton->joint_count];
  for (int i = 0; i < skeleton->joint_count; i++)
  {
    parent_transforms[i].identity();
  }
  for (u32 i = 0; i < skeleton->joint_count; i++)
  {
    Joint* joint             = &skeleton->joints[i];

    Mat44  current_local     = current_poses[i];
    Mat44  parent_transform  = parent_transforms[joint->m_iParent];

    Mat44  current_transform = parent_transform.mul(current_local);
    parent_transforms[i]     = current_transform;
    transforms[i]            = current_transform.mul(joint->m_invBindPose);
  }
  shader.set_mat4("jointTransforms", &transforms[0].m[0], skeleton->joint_count);
}

int main()
{

  AnimationModel model = {};
  if (!parse_gltf(&model, "./data/unarmed_still.glb"))
  {
    printf("Failed to parse model!\n");
    return 1;
  }

  const int     screen_width  = 620;
  const int     screen_height = 480;

  InputState    input_state   = {};

  SDL_GLContext context;
  SDL_Window*   window;
  sta_init_sdl_gl(&window, &context, screen_width, screen_height);
  u32        ticks = 0;

  Shader     shader("./shaders/animation.vert", "./shaders/animation.frag");

  TargaImage image = {};
  sta_read_targa_from_file_rgba(&image, "./data/unarmed_man/Peasant_Man_diffuse.tga");

  GLuint vao, vbo, ebo;
  sta_glGenVertexArrays(1, &vao);
  sta_glGenBuffers(1, &vbo);
  sta_glGenBuffers(1, &ebo);

  sta_glBindVertexArray(vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, vbo);
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(SkinnedVertex) * model.vertex_count, model.vertices, GL_STATIC_DRAW);

  sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * model.index_count, model.indices, GL_STATIC_DRAW);

  int size = sizeof(float) * 16;
  sta_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, size, (void*)0);
  sta_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, size, (void*)(3 * sizeof(float)));
  sta_glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, size, (void*)(5 * sizeof(float)));
  sta_glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, size, (void*)(8 * sizeof(float)));
  sta_glVertexAttribIPointer(4, 4, GL_INT, size, (void*)(12 * sizeof(int)));
  sta_glEnableVertexAttribArray(0);
  sta_glEnableVertexAttribArray(1);
  sta_glEnableVertexAttribArray(2);
  sta_glEnableVertexAttribArray(3);
  sta_glEnableVertexAttribArray(4);

  sta_glBindVertexArray(vao);

  Mat44 view = {};
  view.identity();
  view = view.scale(0.005f);
  view = view.rotate_y(90.0f);
  view = view.rotate_z(90.0f);
  view = view.translate(Vector3(-0.2f, -0.2f, 0));

  shader.use();
  shader.set_int("texture1", 0);

  u32 texture;
  sta_glGenTextures(1, &texture);
  sta_glBindTexture(GL_TEXTURE_2D, texture);

  sta_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data);
  sta_glGenerateMipmap(GL_TEXTURE_2D);

  glActiveTexture(GL_TEXTURE0);
  sta_glBindTexture(GL_TEXTURE_2D, texture);

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
      // view  = view.rotate_y(1.0f);
      // view  = view.rotate_x(1.0f);
      shader.set_mat4("view", view);
    }
    update_animation(model, shader, ticks);

    sta_gl_clear_buffer(1.0, 1.0, 1.0, 1.0);
    glDrawElements(GL_TRIANGLES, model.index_count, GL_UNSIGNED_INT, 0);

    sta_gl_render(window);
  }
}
