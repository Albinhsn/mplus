#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <strings.h>

#include <x86intrin.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <assert.h>
#include <cfloat>
#include <stdlib.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <SDL2/SDL.h>
#include <byteswap.h>

#include "platform.h"
#include "common.h"
#include "sdl.h"
#include "vector.h"
#include "files.h"
#include "shader.h"
#include "animation.h"
#include "collision.h"
#include "input.h"
#include "renderer.h"

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#define IMGUI_DEFINE_MATH_OPERATORS
#include "common.cpp"

Logger logger;
#include "sdl.cpp"
#include "vector.cpp"
#include "font.cpp"
#include "shader.cpp"
#include "collision.cpp"

#include "../libs/imgui/backends/imgui_impl_opengl3.cpp"
#include "../libs/imgui/backends/imgui_impl_sdl2.cpp"
#include "../libs/imgui/imgui.cpp"
#include "../libs/imgui/imgui_draw.cpp"
#include "../libs/imgui/imgui_tables.cpp"
#include "../libs/imgui/imgui_widgets.cpp"

#include "files.cpp"
#include "animation.cpp"
#include "input.cpp"
#include "renderer.cpp"
#include "gltf.cpp"

struct Sphere
{
  Vector2 position;
  f32     r;
};

struct Camera
{
public:
  Camera()
  {
  }
  Camera(Vector3 t, f32 r, f32 _z)
  {
    translation = t;
    rotation    = r;
    z_rotation  = 0;
    z           = _z;
  }

public:
  Mat44 get_view_matrix()
  {
    Mat44 camera_m = Mat44::identity();
    return camera_m.rotate_y(z_rotation).rotate_x(rotation).translate(Vector3(translation.x, translation.y, z));
  }
  Vector3 translation;
  f32     rotation;
  f32     z;
  f32     z_rotation;
};

struct Entity;
struct Hero;

struct Ability
{
  const char* name;
  bool        (*use_ability)(Camera* camera, InputState* input, u32 ticks);
  u32         cooldown_ticks;
  u32         cooldown;
};

struct Hero
{
  char*   name;
  Ability abilities[5];
  u32     can_take_damage_tick;
  u32     damage_taken_cd;
  u32     entity;
  bool    can_move;
};

// something needs to push the dude along, this can be velocity in normal update
// basically we don't want to update the players movement/velocity
// and just let the thing run until we hit something or some time/distance has passed? i.e just a command queue thing?

struct AnimationController;

enum EntityType
{
  ENTITY_PLAYER,
  ENTITY_ENEMY,
  ENTITY_PLAYER_PROJECTILE,
  ENTITY_ENEMY_PROJECTILE,
};

struct Entity
{
  Vector2           position;
  f32               r;
  f32               angle;
  Vector2           velocity;
  i32               hp;
  EntityRenderData* render_data;
  EntityType        type;
  bool              visible;
};
struct StaticGeometry
{
  EntityRenderData* render_data;
  Model*            models;
  Vector3*          position;
  u32               count;
};
struct GameState
{
  u32                 score;
  Camera              camera;
  Mat44               projection;
  Renderer            renderer;
  Model*              models;
  u32                 model_count;
  RenderBuffer*       buffers;
  u32                 buffer_count;
  EntityRenderData*   render_data;
  u32                 render_data_count;
  Hero                player;
  Entity*             entities;
  u32                 entity_capacity;
  u32                 entity_count;
  bool                god;
  AnimationController animation_controllers[50];
  u32                 animation_controller_count;
};
GameState game_state;

void      set_animation(AnimationController* controller, u32 animation_index, u32 tick)
{
  AnimationData* data                      = controller->animation_data;

  controller->current_animation            = animation_index;
  controller->current_animation_start_tick = tick;
}
bool set_animation(AnimationController* controller, const char* animation_name, u32 tick)
{
  AnimationData* data = controller->animation_data;
  for (u32 i = 0; i < data->animation_count; i++)
  {
    if (compare_strings(data->animations[i].name, animation_name))
    {
      if (controller->current_animation != i)
      {
        controller->current_animation            = i;
        controller->next_animation_index         = -1;
        controller->current_animation_start_tick = tick;
      }
      return true;
    }
  }
  controller->current_animation = -1;
  logger.error("Can't set animation of name '%s', couldn't find it!", animation_name);
  return false;
}

bool is_walking_animation(const char* name)
{
  const char* walking_animations[] = {
      "walking",
      "walking_back",
      "walking_left",
      "walking_right",
  };
  for (u32 i = 0; i < ArrayCount(walking_animations); i++)
  {
    if (compare_strings(walking_animations[i], name))
    {
      return true;
    }
  }

  return false;
}

void update_animation_to_walking(AnimationController* controller, u32 tick, const char* direction)
{
  AnimationData* data      = controller->animation_data;
  char*          anim_name = data->animations[controller->current_animation].name;
  if (compare_strings(anim_name, "idle") || is_walking_animation(anim_name))
  {
    set_animation(controller, direction, tick);
  }
}

void update_animation_to_idling(AnimationController* controller, u32 tick)
{
  AnimationData* data = controller->animation_data;
  if (is_walking_animation(data->animations[controller->current_animation].name))
  {
    set_animation(controller, "idle", tick);
  }
}

AnimationController* animation_controller_create(AnimationData* data)
{
  AnimationController* controller          = &game_state.animation_controllers[game_state.animation_controller_count++];
  controller->current_animation_start_tick = 0;
  controller->current_animation            = 0;
  controller->transforms                   = sta_allocate_struct(Mat44, data->skeleton.joint_count);
  controller->next_animation_index         = -1;
  controller->animation_data               = data;
  set_animation(controller, "idle", 0);
  return controller;
}

void update_animations(u32 tick)
{
  for (u32 i = 0; i < game_state.animation_controller_count; i++)
  {
    AnimationController* controller = &game_state.animation_controllers[i];
    if (controller->current_animation == -1)
    {
      if (!set_animation(controller, "idle", tick))
      {
        for (u32 j = 0; j < controller->animation_data->skeleton.joint_count; j++)
        {
          controller->transforms[j] = Mat44::identity();
        }
      }
      continue;
    }

    Animation* current_animation = &controller->animation_data->animations[controller->current_animation];

    if (tick < controller->current_animation_start_tick)
    {

      logger.error("%d < %d", tick, controller->current_animation_start_tick);
      assert(tick >= controller->current_animation_start_tick && "How did you manage this");
    }
    if (current_animation->scaling * current_animation->duration * 1000 < tick - controller->current_animation_start_tick)
    {
      if (controller->next_animation_index == -1)
      {
        // ToDo interpolate between changes in poses
        set_animation(controller, "idle", tick);
      }
      else
      {
        set_animation(controller, controller->next_animation_index, tick);
      }
      controller->next_animation_index         = -1;
      controller->current_animation_start_tick = tick;
    }
    update_animation(&controller->animation_data->skeleton, current_animation, controller->transforms, tick - controller->current_animation_start_tick);
  }
}

#include "ui.cpp"
#include "ui.h"

u32 get_buffer_by_name(const char* filename)
{
  for (u32 i = 0; i < game_state.buffer_count; i++)
  {
    if (compare_strings(game_state.buffers[i].model_name, filename))
    {
      return game_state.buffers[i].buffer_id;
    }
  }
  logger.error("Couldn't find buffer '%s'", filename);
  assert(!"Couldn't find the buffer!");
}

ModelFileExtensions get_model_file_extension(char* file_name)
{
  u32 len = strlen(file_name);
  if (len > 4 && compare_strings("obj", &file_name[len - 3]))
  {
    return MODEL_FILE_OBJ;
  }
  if (len > 4 && compare_strings("glb", &file_name[len - 3]))
  {
    return MODEL_FILE_GLB;
  }
  if (len > 5 && compare_strings("anim", &file_name[len - 4]))
  {
    return MODEL_FILE_ANIM;
  }

  return MODEL_FILE_UNKNOWN;
}
Model* get_model_by_name(const char* filename)
{
  for (u32 i = 0; i < game_state.model_count; i++)
  {
    if (compare_strings(filename, game_state.models[i].name))
    {
      return &game_state.models[i];
    }
  }
  logger.error("Didn't find model '%s'", filename);
  return 0;
}

EntityRenderData* get_render_data_by_name(const char* name)
{
  for (u32 i = 0; i < game_state.render_data_count; i++)
  {
    if (compare_strings(game_state.render_data[i].name, name))
    {
      return &game_state.render_data[i];
    }
  }
  logger.error("Couldn't find render data with name '%s'", name);
  assert(!"Couldn't find render data!");
}

bool load_models_from_files(const char* file_location)
{

  Buffer buffer = {};
  if (!sta_read_file(&buffer, file_location))
  {
    return false;
  }
  Json json = {};
  if (!sta_json_deserialize_from_string(&buffer, &json))
  {
    return false;
  };
  assert(json.headType == JSON_OBJECT && "Expected head to be object");
  JsonObject* head       = &json.obj;

  u32         count      = head->size;

  game_state.model_count = count;

  game_state.models      = sta_allocate_struct(Model, game_state.model_count);
  logger.info("Found %d models", game_state.model_count);
  for (u32 i = 0; i < game_state.model_count; i++)
  {
    // ToDo free the loaded memory?
    JsonObject*         json_data      = head->values[i].obj;
    char*               model_name     = head->keys[i];
    char*               model_location = json_data->lookup_value("location")->string;
    ModelFileExtensions extension      = get_model_file_extension(model_location);
    Model*              model          = &game_state.models[i];
    model->name                        = model_name;
    switch (extension)
    {
    case MODEL_FILE_OBJ:
    {
      ModelData model_data = {};
      if (!sta_parse_wavefront_object_from_file(&model_data, model_location))
      {
        logger.error("Failed to parse obj from '%s'", model_location);
      };

      model->index_count  = model_data.vertex_count;
      model->vertex_count = model_data.vertex_count;
      model->vertices     = sta_allocate_struct(Vector3, model_data.vertex_count);
      for (u32 j = 0; j < model_data.vertex_count; j++)
      {
        model->vertices[j] = model_data.vertices[j].vertex;
      }
      model->indices          = model_data.indices;
      model->animation_data   = 0;
      model->vertex_data      = (void*)model_data.vertices;
      model->vertex_data_size = sizeof(VertexData);
      break;
    }
    case MODEL_FILE_GLB:
    {
      AnimationModel model_data = {};
      if (!gltf_parse(&model_data, model_location))
      {
        logger.error("Failed to read glb from '%s'", model_location);
      }
      model->indices          = model_data.indices;
      model->index_count      = model_data.index_count;
      model->vertex_count     = model_data.vertex_count;
      model->vertex_data_size = sizeof(SkinnedVertex);
      model->vertex_data      = (void*)model_data.vertices;
      model->vertices         = sta_allocate_struct(Vector3, model_data.vertex_count);

      for (u32 j = 0; j < model_data.vertex_count; j++)
      {
        model->vertices[j] = model_data.vertices[j].position;
      }
      model->animation_data           = (AnimationData*)sta_allocate_struct(AnimationData, 1);
      model->animation_data->skeleton = model_data.skeleton;
      // ToDo this should change once you fixed the parser
      model->animation_data->animation_count = 1;
      model->animation_data->animations      = (Animation*)sta_allocate_struct(Animation, model->animation_data->animation_count);
      model->animation_data->animations      = model_data.animations;
      model->animation_data->animation_count = model_data.animation_count;
      break;
    }
    case MODEL_FILE_ANIM:
    {
      AnimationModel model_data       = {};
      JsonValue*     mapping_location = json_data->lookup_value("mapping");
      char*          loc              = mapping_location ? mapping_location->string : 0;
      if (!parse_animation_file(&model_data, model_location, loc))
      {
        logger.error("Failed to read anim from '%s'", model_location);
        continue;
      }
      model->indices          = model_data.indices;
      model->index_count      = model_data.index_count;
      model->vertex_count     = model_data.vertex_count;
      model->vertex_data_size = sizeof(SkinnedVertex);
      model->vertex_data      = (void*)model_data.vertices;
      model->vertices         = sta_allocate_struct(Vector3, model_data.vertex_count);

      for (u32 j = 0; j < model_data.vertex_count; j++)
      {
        model->vertices[j] = model_data.vertices[j].position;
      }
      model->animation_data           = (AnimationData*)sta_allocate_struct(AnimationData, 1);
      model->animation_data->skeleton = model_data.skeleton;
      // ToDo if we free the memory :)
      model->animation_data->animations      = model_data.animations;
      model->animation_data->animation_count = model_data.animation_count;
      break;
    }
    case MODEL_FILE_UNKNOWN:
    {
      assert(!"Unknown model!");
    }
    }
    logger.info("Loaded %s from '%s'", model_name, model_location);
  }
  return true;
}

bool load_buffers_from_files(const char* file_location)
{

  Buffer buffer = {};
  if (!sta_read_file(&buffer, file_location))
  {
    return false;
  }
  Json json = {};
  if (!sta_json_deserialize_from_string(&buffer, &json))
  {
    return false;
  };
  assert(json.headType == JSON_OBJECT && "Expected head to be object");
  JsonObject* head        = &json.obj;

  u32         count       = head->size;

  game_state.buffers      = (RenderBuffer*)sta_allocate_struct(RenderBuffer, count);
  game_state.buffer_count = count;

  for (u32 i = 0; i < count; i++)
  {
    char*            model_name             = head->keys[i];
    Model*           model                  = get_model_by_name(model_name);
    JsonObject*      model_json             = head->values[i].obj;
    JsonArray*       attributes_json        = model_json->lookup_value("attributes")->arr;
    u32              buffer_attribute_count = attributes_json->arraySize;
    BufferAttributes attributes[buffer_attribute_count];
    for (u32 j = 0; j < buffer_attribute_count; j++)
    {
      attributes[j].count = attributes_json->values[j].obj->lookup_value("count")->number;
      attributes[j].type  = (BufferAttributeType)attributes_json->values[j].obj->lookup_value("type")->number;
    }
    game_state.buffers[i].model_name = model_name;
    game_state.buffers[i].buffer_id  = game_state.renderer.create_buffer_from_model(model, attributes, buffer_attribute_count);
    logger.info("Loaded buffer '%s', id: %d", model_name, game_state.buffers[i].buffer_id);
  }
  return true;
}

u32 get_new_entity()
{
  RESIZE_ARRAY(game_state.entities, Entity, game_state.entity_count, game_state.entity_capacity);
  return game_state.entity_count++;
}

Vector2 closest_point_triangle(Triangle triangle, Vector2 p)
{
  Vector2 ab = triangle.points[1].sub(triangle.points[0]);
  Vector2 ac = triangle.points[2].sub(triangle.points[0]);
  Vector2 ap = p.sub(triangle.points[0]);

  float   d1 = ab.dot(ap);
  float   d2 = ac.dot(ap);
  if (d1 <= 0.0f && d2 <= 0.0f)
  {
    return triangle.points[0];
  }

  Vector2 bp = p.sub(triangle.points[1]);
  float   d3 = ab.dot(bp);
  float   d4 = ac.dot(bp);
  if (d3 >= 0.0f && d4 <= d3)
  {
    return triangle.points[1];
  }

  float vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
  {
    float v = d1 / (d1 - d3);
    ab.scale(v);
    return Vector2(triangle.points[0].x + ab.x, triangle.points[0].y + ab.y);
  }

  Vector2 cp = p.sub(triangle.points[2]);
  float   d5 = ab.dot(cp);
  float   d6 = ac.dot(cp);
  if (d6 >= 0.0f && d5 <= d6)
  {
    return triangle.points[2];
  }

  float vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
  {
    float w = d2 / (d2 - d6);
    ac.scale(w);
    return Vector2(triangle.points[0].x + ac.x, triangle.points[0].y + ac.y);
  }

  float va = d3 * d6 - d5 * d4;
  if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
  {
    float   w  = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    Vector2 bc = triangle.points[2].sub(triangle.points[1]);
    bc.scale(w);
    return Vector2(triangle.points[1].x + bc.x, triangle.points[1].y + bc.y);
  }

  float denom = 1.0f / (va + vb + vc);
  float v     = vb * denom;
  float w     = vc * denom;
  ab.scale(v);
  ac.scale(w);
  float x = triangle.points[0].x + ab.x + ac.x;
  float y = triangle.points[0].y + ab.y + ac.y;
  return Vector2(x, y);
}

u32 get_tile_count_per_row()
{
  // 2.0f for the length of the thing divided by the radius *  aka 2.0f / (r * 2) == 1 /r
  return 24;
}

f32 get_tile_size()
{
  return 2.0f / get_tile_count_per_row();
}

bool collides_with_static_geometry(Vector2& closest_point, Vector2 position, f32 r);
struct Map
{
public:
  Map()
  {
  }
  u32*           indices;
  Vector2*       vertices;
  u32            index_count;
  u32            vertex_count;
  StaticGeometry static_geometry;
  void           init_map(Model* model)
  {
    this->index_count    = model->vertex_count;
    this->vertex_count   = model->vertex_count;
    this->indices        = model->indices;
    VertexData* vertices = (VertexData*)model->vertex_data;
    this->vertices       = sta_allocate_struct(Vector2, vertex_count);
    for (u32 i = 0; i < vertex_count; i++)
    {
      Vector3 v         = vertices[i].vertex;
      this->vertices[i] = Vector2(v.x, v.y);
    }
  }

  void get_tile_position(u8& tile_x, u8& tile_y, Vector2 position)
  {
    u32 tile_count = get_tile_count_per_row();

    f32 px_n       = ((1.0f + position.x) * 0.5f);
    f32 py_n       = ((1.0f + position.y) * 0.5f);
    tile_x         = px_n * tile_count;
    tile_y         = py_n * tile_count;
  }
};
Map  map;
bool collides_with_static_geometry(Vector2& closest_point, Vector2 position, f32 r)
{
  for (u32 i = 0; i < map.static_geometry.count; i++)
  {
    Model*   model    = &map.static_geometry.models[i];
    f32      scale    = map.static_geometry.render_data[i].scale;
    Vector3  pos      = map.static_geometry.position[i];

    Vector3* vertices = model->vertices;
    u32*     indices  = model->indices;
    for (u32 j = 0; j < model->index_count; j += 3)
    {
      Triangle t;
      t.points[0].x = vertices[indices[j]].x * scale + pos.x;
      t.points[0].y = vertices[indices[j]].y * scale + pos.y;
      t.points[1].x = vertices[indices[j + 1]].x * scale + pos.x;
      t.points[1].y = vertices[indices[j + 1]].y * scale + pos.y;
      t.points[2].x = vertices[indices[j + 2]].x * scale + pos.x;
      t.points[2].y = vertices[indices[j + 2]].y * scale + pos.y;
      closest_point = closest_point_triangle(t, position);
      if (closest_point.sub(position).len() < r)
      {
        return true;
      }
    }
  }
  return false;
}
bool load_static_geometry_from_file(const char* filename)
{
  Buffer buffer = {};
  if (!sta_read_file(&buffer, filename))
  {
    return false;
  }
  Json json = {};
  if (!sta_json_deserialize_from_string(&buffer, &json))
  {
    return false;
  };
  assert(json.headType == JSON_OBJECT && "Expected head to be object");
  JsonObject* head                = &json.obj;

  map.static_geometry.count       = head->size;
  map.static_geometry.position    = (Vector3*)sta_allocate_struct(Vector3, map.static_geometry.count);
  map.static_geometry.models      = (Model*)sta_allocate_struct(Model, map.static_geometry.count);
  map.static_geometry.render_data = (EntityRenderData*)sta_allocate_struct(EntityRenderData, map.static_geometry.count);

  logger.info("Found %d static geometry items", map.static_geometry.count);
  for (u32 i = 0; i < map.static_geometry.count; i++)
  {
    map.static_geometry.render_data[i] = *get_render_data_by_name(head->keys[i]);
    map.static_geometry.models[i]      = *get_model_by_name(head->values[i].obj->lookup_value("model")->string);
    JsonArray* position_json           = head->values[i].obj->lookup_value("position")->arr;
    map.static_geometry.position[i].x  = position_json->values[0].number;
    map.static_geometry.position[i].y  = position_json->values[1].number;
    map.static_geometry.position[i].z  = position_json->values[2].number;
    logger.info("Item %d: '%s' at (%f, %f, %f)", i, map.static_geometry.render_data[i].name, map.static_geometry.position[i].x, map.static_geometry.position[i].y, map.static_geometry.position[i].z);
  }

  return true;
}

bool load_map_from_file(const char* filename)
{
  Buffer buffer = {};
  if (!sta_read_file(&buffer, filename))
  {
    return false;
  }
  Json json = {};
  if (!sta_json_deserialize_from_string(&buffer, &json))
  {
    return false;
  };
  assert(json.headType == JSON_OBJECT && "Expected head to be object");
  JsonObject* head  = &json.obj;

  Model*      model = get_model_by_name(head->lookup_value("model")->string);
  f32         scale = head->lookup_value("model")->number;
  if (!load_static_geometry_from_file(head->lookup_value("static_geometry")->string))
  {
    logger.error("Failed to read static geometry data from '%s'", head->lookup_value("static_geometry")->string);
    return false;
  }
  map.init_map(model);
  return true;
}

struct HeapNode
{
public:
  u16  distance;
  u16* path;
  u16  path_count;
  u16  path_capacity;
};

struct Heap
{
public:
  HeapNode remove()
  {
    HeapNode out = nodes[0];
    nodes[0]     = nodes[node_count - 1];
    node_count--;
    heapify_down(0);

    return out;
  }
  void insert(HeapNode node)
  {
    if (node_count >= node_capacity)
    {
      node_capacity *= 2;
      nodes = (HeapNode*)realloc(nodes, sizeof(HeapNode) * node_capacity);
    }
    nodes[node_count] = node;
    heapify_up(node_count);
    node_count++;
  }
  void heapify_down(u16 idx)
  {

    u16 lIdx = left_child(idx);
    u16 rIdx = right_child(idx);

    if (idx >= node_count || lIdx >= node_count)
    {
      return;
    }

    HeapNode lV  = nodes[lIdx];
    HeapNode v   = nodes[idx];
    u32      lVD = lV.distance + lV.path_count;
    u32      vD  = v.distance + v.path_count;
    if (rIdx >= node_count)
    {

      if (vD > lVD)
      {
        nodes[lIdx] = v;
        nodes[idx]  = lV;
      }
      return;
    }
    HeapNode rV  = nodes[rIdx];
    u32      rVD = rV.distance + rV.path_count;

    if (lVD <= rVD && lVD < vD)
    {
      nodes[lIdx] = v;
      nodes[idx]  = lV;
      heapify_down(lIdx);
    }
    else if (rVD < lVD && rVD < vD)
    {
      nodes[rIdx] = v;
      nodes[idx]  = rV;
      heapify_down(rIdx);
    }
  }
  void heapify_up(u16 idx)
  {
    if (idx == 0)
    {
      return;
    }

    u32      pIdx = parent(idx);
    HeapNode pV   = nodes[pIdx];
    HeapNode v    = nodes[idx];
    u32      pVD  = pV.distance + pV.path_count;
    u32      vD   = v.distance + v.path_count;

    if (pVD > vD)
    {
      nodes[idx]  = pV;
      nodes[pIdx] = v;
      heapify_up(pIdx);
    }
  }
  u16 left_child(u16 idx)
  {
    return 2 * idx + 1;
  }
  u16 right_child(u16 idx)
  {
    return 2 * idx + 2;
  }
  u16 parent(u16 idx)
  {
    return (idx - 1) / 2;
  }
  bool have_visited(HeapNode curr)
  {
    u16 position = curr.path[curr.path_count - 1];
    for (u32 i = 0; i < visited_count; i++)
    {
      if (visited[i] == position)
      {
        distances[i] = MIN(distances[i], curr.distance + curr.path_count);
        return distances[i] <= curr.distance + curr.path_count;
      }
    }
    if (visited_count >= visited_capacity)
    {
      visited_capacity *= 2;
      visited   = (u16*)realloc(visited, sizeof(u16) * visited_capacity);
      distances = (u32*)realloc(distances, sizeof(u32) * visited_capacity);
    }
    visited[visited_count]     = position;
    distances[visited_count++] = curr.path_count + curr.distance;
    return false;
  }
  HeapNode* nodes;
  u16*      visited;
  u32*      distances;
  u32       visited_count;
  u32       visited_capacity;
  u32       node_count;
  u32       node_capacity;
};
struct Path
{
  u16* path;
  u32  path_count;
};

bool is_out_of_map_bounds(Vector2 position, f32 r)
{
  for (u32 i = 0; i < map.vertex_count - 2; i += 3)
  {
    Triangle t;
    t.points[0].x = map.vertices[map.indices[i]].x;
    t.points[0].y = map.vertices[map.indices[i]].y;
    t.points[1].x = map.vertices[map.indices[i + 1]].x;
    t.points[1].y = map.vertices[map.indices[i + 1]].y;
    t.points[2].x = map.vertices[map.indices[i + 2]].x;
    t.points[2].y = map.vertices[map.indices[i + 2]].y;
    if (point_in_triangle_2d(t, Point2(position.x, position.y)))
    {
      return false;
    }
  }
  return true;
}
bool is_out_of_map_bounds(Vector2& closest_point, Vector2 position, f32 r)
{
  f32 distance = FLT_MAX;
  for (u32 i = 0; i < map.vertex_count - 2; i += 3)
  {
    Triangle t;
    t.points[0].x = map.vertices[map.indices[i]].x;
    t.points[0].y = map.vertices[map.indices[i]].y;
    t.points[1].x = map.vertices[map.indices[i + 1]].x;
    t.points[1].y = map.vertices[map.indices[i + 1]].y;
    t.points[2].x = map.vertices[map.indices[i + 2]].x;
    t.points[2].y = map.vertices[map.indices[i + 2]].y;
    Vector2 cp    = closest_point_triangle(t, position);
    f32     len   = cp.sub(position).len();
    if (len < r)
    {
      return false;
    }
    if (len < distance)
    {
      closest_point = cp;
      distance      = len;
    }
  }
  return true;
}
f32 tile_position_to_game(i16 x)
{
  u32 count = get_tile_count_per_row();
  return (x + 0.5f) / (f32)count * 2.0f - 1.0f;
}
f32 tile_position_to_game2(i16 x)
{
  u32 count = get_tile_count_per_row();
  return x / (f32)count * 2.0f - 1.0f;
}

bool IntersectSegmentTriangle(Vector3 q, Vector3 p, Triangle3D triangle, float& u, float& v, float& w, float& t)
{
  Vector3 ab = triangle.points[1].sub(triangle.points[0]);
  Vector3 ac = triangle.points[2].sub(triangle.points[0]);
  Vector3 qp = p.sub(q);

  Vector3 n  = ab.cross(ac);

  float   d  = qp.dot(n);

  Vector3 ap = p.sub(triangle.points[0]);
  t          = ap.dot(n);

  Vector3 e  = qp.cross(ap);
  v          = ac.dot(e);

  w          = -ab.dot(e);
  bool out   = true;
  if (d <= 0.0f)
  {
    out = false;
  }
  if (t < 0.0f)
  {
    out = false;
  }
  if (v < 0.0f || v > d)
  {
    out = false;
  }
  if (w < 0.0f || v + w > d)
  {
    out = false;
  }

  float ood = 1.0f / d;
  t *= ood;
  v *= ood;
  w *= ood;
  u = 1.0f - v - w;
  return out;
}

float Signed2DTriArea(Vector2 a, Vector2 b, Vector2 c)
{
  return (a.x - c.x) * (b.y - c.y) - (a.y - c.y) * (b.x - c.x);
}
bool Test2DSegmentSegment(Vector2 a, Vector2 b, Vector2 c, Vector2 d)
{

  float a1 = Signed2DTriArea(a, b, d);
  float a2 = Signed2DTriArea(a, b, c);

  if (a1 != 0 && a2 != 0 && a1 * a2 < 0.0f)
  {
    float a3 = Signed2DTriArea(c, d, a);

    float a4 = a3 + a2 - a1;

    if (a3 * a4 < 0.0f)
    {
      return true;
    }
  }
  return false;
}

bool is_tile_in_map(u8 x, u8 y, f32 r)
{
  f32     xs = tile_position_to_game(x);
  f32     ys = tile_position_to_game(y);

  Vector2 v0(xs, ys);

  return !is_out_of_map_bounds(v0, r);
}

void find_path(Path* path, Vector2 needle, Vector2 source, f32 r)
{
  // get the tile position of both source and needle
  u8 source_x, source_y;
  u8 needle_x, needle_y;
  map.get_tile_position(source_x, source_y, source);
  map.get_tile_position(needle_x, needle_y, needle);

  // init heap
  Heap heap               = {};
  heap.node_count         = 0;
  heap.node_capacity      = 2;
  heap.nodes              = (HeapNode*)malloc(sizeof(HeapNode) * heap.node_capacity);

  HeapNode init_node      = {};
  init_node.path_capacity = 2;
  init_node.path_count    = 1;
  init_node.path          = (u16*)malloc(sizeof(u16) * init_node.path_capacity);
  init_node.path[0]       = (source_x << 8) | source_y;
  // should include number of steps
  init_node.distance = ABS((i16)(source_x - needle_x)) + ABS((i16)(source_y - needle_y));

  heap.insert(init_node);

  heap.visited_count    = 0;
  heap.visited_capacity = 2;
  heap.visited          = (u16*)malloc(sizeof(u16) * heap.visited_capacity);
  heap.distances        = (u32*)malloc(sizeof(u32) * heap.visited_capacity);

  u32 tile_count        = get_tile_count_per_row();

  path->path_count      = 0;
  while (heap.node_count != 0)
  {
    HeapNode curr = heap.remove();
    assert(curr.path_count <= curr.path_capacity && "How?");
    u16 curr_pos = curr.path[curr.path_count - 1];
    if (heap.have_visited(curr))
    {
      continue;
    }

    if (needle_x == (curr_pos >> 8) && (needle_y == (curr_pos & 0xFF)))
    {
      for (u32 i = 0; i < curr.path_count; i++)
      {
        path->path[i] = curr.path[i];
      }
      path->path_count = curr.path_count;
      free(curr.path);
      for (u32 i = 0; i < heap.node_count; i++)
      {
        HeapNode curr = heap.nodes[i];
        free(curr.path);
      }
      free(heap.nodes);
      return;
    }
    // add neighbours if needed
    i8 XY[8][2] = {
        { 1,  0},
        { 0, -1},
        {-1,  0},
        { 0,  1},
        { 1,  1},
        {-1,  1},
        { 1, -1},
        {-1, -1},
    };
    for (u32 i = 0; i < 8; i++)
    {
      i16  x         = (curr_pos >> 8) + XY[i][0];
      i16  y         = (curr_pos & 0xFF) + XY[i][1];
      u16  new_pos   = (x << 8) | y;
      bool is_target = needle_x == x && needle_y == y;
      if (!is_target)
      {
        if (x < 0 || y < 0 || x >= (i16)tile_count || y >= (i16)tile_count)
        {
          continue;
        }
        Vector2 c;
        if (!is_tile_in_map(x, y, r) || collides_with_static_geometry(c, Vector2(tile_position_to_game(x), tile_position_to_game(y)), r))
        {
          continue;
        }
      }
      HeapNode new_node      = {};
      new_node.path          = (u16*)malloc(sizeof(u16) * curr.path_capacity);
      new_node.path_capacity = curr.path_capacity;
      for (u32 j = 0; j < curr.path_count; j++)
      {
        new_node.path[j] = curr.path[j];
      }
      new_node.distance = ABS((i16)(x - needle_x)) + ABS((i16)(y - needle_y));
      // if so create new with same path and add the neighbour coordinates
      new_node.path_count = curr.path_count;
      if (new_node.path_count >= new_node.path_capacity)
      {
        new_node.path_capacity *= 2;
        new_node.path = (u16*)realloc(new_node.path, sizeof(u16) * new_node.path_capacity);
      }
      new_node.path[new_node.path_count++] = new_pos;
      assert(new_node.path_count <= tile_count * 4 && "Out of range for path!");
      heap.insert(new_node);
    }
    // free node memory :)
    free(curr.path);
  }

  logger.error("Didn't find a path to player from (%d, %d) to (%d, %d), (%f, %f), (%f, %f)", source_x, source_y, needle_x, needle_y, source.x, source.y, needle.x, needle.y);
  assert(!"Didn't find a path to player!");
}

bool sphere_sphere_collision(Sphere s0, Sphere s1)
{
  f32 x_diff                           = ABS(s0.position.x - s1.position.x);
  f32 y_diff                           = ABS(s0.position.y - s1.position.y);
  f32 distance_between_centers_squared = x_diff * x_diff + y_diff * y_diff;
  f32 radii_diff                       = (s0.r + s1.r) * (s0.r + s1.r);
  return distance_between_centers_squared <= radii_diff;
}
TargaImage noise;

bool       is_valid_position(Vector2 position, f32 r)
{
  Vector2 closest;
  return !(is_out_of_map_bounds(closest, position, r) || collides_with_static_geometry(closest, position, r));
}

Vector2 get_random_position(u32 tick, u32 enemy_counter, f32 r)
{
  u32     SEED_X             = 1234;
  u32     SEED_Y             = 2345;

  f32     min_valid_distance = 0.5f;

  Vector2 position           = {};

  Vector2 player_position    = game_state.entities[game_state.player.entity].position;

  f32     distance_to_player;
  Vector2 p;
  do
  {
    // ToDo check vs player
    u32 offset_x = ((tick + enemy_counter) * SEED_X) % (noise.width * noise.height);
    u32 offset_y = ((tick + enemy_counter) * SEED_Y) % (noise.width * noise.height);

    u8  x        = noise.data[4 * offset_x];
    u8  y        = noise.data[4 * offset_y];

    tick *= 3;

    position           = Vector2((x / 255.0f) * 2.0f - 1.0f, (y / 255.0f) * 2.0f - 1.0f);

    distance_to_player = position.sub(player_position).len();

  } while (!is_valid_position(position, r) || distance_to_player < min_valid_distance);

  return position;
}

enum EnemyType
{
  ENEMY_MELEE,
  ENEMY_RANGED,
};

struct Enemy
{
  u32       entity;
  u32       initial_hp;
  u32       cooldown;
  u32       cooldown_timer;
  Path      path;
  EnemyType type;
  bool      can_move;
};
struct Wave
{
  u32*   spawn_times;
  u64    spawn_count;
  u64    enemy_count;
  u64    enemies_alive;
  Enemy* enemies;
};

struct CommandFindPathData
{
  Enemy* enemy;
  u32    tick;
};

enum CommandType
{
  CMD_SPAWN_ENEMY,
  CMD_LET_RANGED_MOVE_AFTER_SHOOTING,
  CMD_EXPLODE_PILLAR,
  CMD_FIND_PATH,
  CMD_STOP_CHARGE
};

struct Command
{
  CommandType type;
  void*       data;
  u32         execute_tick;
};

struct CommandNode
{
  Command      command;
  CommandNode* next;
};

// ToDo make it a heap?
struct CommandQueue
{
  u32           cmds;
  PoolAllocator pool;
  CommandNode*  head;
  CommandNode*  tail;
};
struct CommandSpawnEnemyData
{
  Wave* wave;
  u32   enemy_index;
};

struct CommandLetRangedMoveAfterShooting
{
  Enemy* enemy;
};

CommandQueue command_queue;
struct CommandExplodePillarOfFlameData
{
  Vector2 position;
};

void add_command(CommandType type, void* data, u32 tick);
void spawn(Wave* wave, u32 enemy_index)
{
  Enemy*  enemy  = &wave->enemies[enemy_index];
  Entity* entity = &game_state.entities[enemy->entity];
  wave->spawn_count |= enemy_index == 0 ? 1 : ((u64)1 << (u64)enemy_index);
  wave->enemies_alive++;
  EntityRenderData* render_data = entity->render_data;
  entity->position              = get_random_position(wave->spawn_times[enemy_index], enemy_index, entity->r);
  entity->hp                    = enemy->initial_hp;
  logger.info("Spawning enemy %d at (%f, %f)", enemy_index, entity->position.x, entity->position.y);

  CommandFindPathData* path_data = sta_allocate_struct(CommandFindPathData, 1);
  path_data->enemy               = enemy;
  path_data->tick                = wave->spawn_times[enemy_index];
  entity->visible                = true;

  add_command(CMD_FIND_PATH, (void*)path_data, path_data->tick);
}

void add_command(CommandType type, void* data, u32 tick)
{
  command_queue.cmds++;
  CommandNode* node          = (CommandNode*)command_queue.pool.alloc();
  node->command.type         = type;
  node->command.data         = data;
  node->command.execute_tick = tick;

  if (!command_queue.tail)
  {
    node->next         = command_queue.head;
    command_queue.head = node;
    command_queue.tail = node;
  }
  else
  {
    command_queue.tail->next = node;
    command_queue.tail       = node;
  }
}

void run_command_explode_pillar_of_flame(void* data)
{
  CommandExplodePillarOfFlameData* pof_data = (CommandExplodePillarOfFlameData*)data;
  Sphere                           pof_sphere;
  pof_sphere.position = pof_data->position;
  pof_sphere.r        = 0.2f;
  // iterate over the entities
  for (u32 i = 0; i < game_state.entity_count; i++)
  {
    if (game_state.entities[i].type == ENTITY_ENEMY && game_state.entities[i].visible)
    {
      Sphere enemy_sphere;
      enemy_sphere.position = game_state.entities[i].position;
      enemy_sphere.r        = game_state.entities[i].r;
      if (sphere_sphere_collision(pof_sphere, enemy_sphere))
      {
        game_state.score += 100;
        game_state.entities[i].hp = 0;
      }
    }
  }
}

void run_command_spawn_enemy(void* data)
{
  CommandSpawnEnemyData* enemy_data = (CommandSpawnEnemyData*)data;
  spawn(enemy_data->wave, enemy_data->enemy_index);
}

void run_command_shoot_arrow(void* data)
{
  CommandLetRangedMoveAfterShooting* arrow_data = (CommandLetRangedMoveAfterShooting*)data;
  Enemy*                             enemy      = arrow_data->enemy;
  enemy->can_move                               = true;
}

void run_command_find_path(void* data)
{
  const u32            next_tick    = 75;
  CommandFindPathData* path_data    = (CommandFindPathData*)data;
  path_data->enemy->path.path_count = 0;
  find_path(&path_data->enemy->path, game_state.entities[game_state.player.entity].position, game_state.entities[path_data->enemy->entity].position, game_state.entities[path_data->enemy->entity].r);
  path_data->tick += next_tick;
  add_command(CMD_FIND_PATH, (void*)path_data, path_data->tick);
}

void run_command_stop_charge()
{
  game_state.player.can_move = true;
}

void run_commands(u32 ticks)
{

  CommandNode* node = command_queue.head;
  CommandNode* prev = 0;
  while (node)
  {
    if (ticks > node->command.execute_tick)
    {
      switch (node->command.type)
      {
      case CMD_EXPLODE_PILLAR:
      {
        run_command_explode_pillar_of_flame(node->command.data);
        break;
      }
      case CMD_SPAWN_ENEMY:
      {
        run_command_spawn_enemy(node->command.data);
        break;
      }
      case CMD_LET_RANGED_MOVE_AFTER_SHOOTING:
      {
        run_command_shoot_arrow(node->command.data);
        break;
      }
      case CMD_FIND_PATH:
      {
        run_command_find_path(node->command.data);
        break;
      }
      case CMD_STOP_CHARGE:
      {
        run_command_stop_charge();
        break;
      }
      }

      if (prev)
      {
        prev->next = node->next;
      }
      else
      {
        command_queue.head = node->next;
      }
      if (node == command_queue.tail)
      {
        command_queue.tail = prev;
      }

      CommandNode* next = node->next;
      command_queue.pool.free((u64)node);
      node = next;
      command_queue.cmds--;
      continue;
    }
    prev = node;
    node = node->next;
  }
}

void handle_player_movement(Camera& camera, InputState* input, u32 tick)
{
  Entity*              entity      = &game_state.entities[game_state.player.entity];
  EntityRenderData*    render_data = entity->render_data;
  AnimationController* controller  = render_data->animation_controller;
  f32*                 mouse_pos   = input->mouse_position;
  entity->angle                    = atan2f(mouse_pos[1] - entity->position.y - camera.translation.y, mouse_pos[0] - entity->position.x - camera.translation.x);
  if (game_state.player.can_move)
  {
    entity->velocity = {};
    f32 MS           = 0.01f;

    if (input->is_key_pressed('w'))
    {
      entity->velocity.y += MS;
    }
    if (input->is_key_pressed('a'))
    {
      entity->velocity.x -= MS;
    }
    if (input->is_key_pressed('s'))
    {
      entity->velocity.y -= MS;
    }
    if (input->is_key_pressed('d'))
    {
      entity->velocity.x += MS;
    }

    if (controller)
    {
      if (entity->velocity.x == 0 && entity->velocity.y == 0)
      {
        update_animation_to_idling(controller, tick);
      }
      else
      {

        f32 angle_difference = entity->angle - atan2f(entity->velocity.y, entity->velocity.x);
        // Uncheck for animation test bed
        // angle_difference     = atan2f(entity->velocity.y, entity->velocity.x) + DEGREES_TO_RADIANS(-90);

        if (ABS(angle_difference) < PI / 4)
        {
          update_animation_to_walking(controller, tick, "walking");
        }
        else if (angle_difference > PI / 4 && angle_difference < (3.0f * PI) / 4.0f)
        {
          update_animation_to_walking(controller, tick, "walking_left");
        }
        else if (angle_difference < -PI / 4 && angle_difference > (-3.0f * PI) / 4.0f)
        {
          update_animation_to_walking(controller, tick, "walking_right");
        }
        else
        {
          update_animation_to_walking(controller, tick, "walking_back");
        }
      }
    }
  }

  camera.translation = Vector3(-entity->position.x, -entity->position.y, 0.0);
}

static inline bool on_cooldown(Ability ability, u32 tick)
{
  return tick < ability.cooldown;
}

void handle_abilities(Camera camera, InputState* input, u32 tick)
{
  const char keybinds[] = {'1', '2', '3', '4'};
  for (u32 i = 0; i < ArrayCount(keybinds); i++)
  {
    if (input->is_key_pressed(keybinds[i]) && !on_cooldown(game_state.player.abilities[i], tick))
    {
      Ability* ability = &game_state.player.abilities[i];
      if (ability->use_ability(&camera, input, tick))
      {
        ability->cooldown = tick + ability->cooldown_ticks;
        logger.info("Using %s", ability->name);
      }
    }
  }
}
bool use_ability_blink(Camera* camera, InputState* input, u32 ticks)
{

  Entity    player_entity = game_state.entities[game_state.player.entity];
  Vector2   position      = player_entity.position;
  f32       angle         = player_entity.angle;

  const f32 max_size      = 0.4f;
  const f32 step_size     = 0.005f;
  const u32 max_steps     = max_size / step_size;
  Vector2   direction     = Vector2(cosf(angle) * step_size, sinf(angle) * step_size);
  for (u32 i = 0; i < max_steps; i++)
  {

    position.x += direction.x;
    position.y += direction.y;
    Vector2 closest_point = {};
    // ToDo This does not take into account the radius?
    if (is_out_of_map_bounds(closest_point, position, player_entity.r))
    {
      position = closest_point;
      break;
    }
    if (collides_with_static_geometry(closest_point, position, player_entity.r))
    {
      position.x -= direction.x;
      position.y -= direction.y;
      break;
    }
  }
  game_state.entities[game_state.player.entity].position = position;
  if (player_entity.render_data->animation_controller)
  {
    set_animation(player_entity.render_data->animation_controller, "blink", ticks);
  }

  return true;
}

bool use_ability_fireball(Camera* camera, InputState* input, u32 ticks)
{
  // ToDo this can reuse dead fireballs?

  u32     entity_index = get_new_entity();
  Entity* e            = &game_state.entities[entity_index];
  e->visible           = true;
  e->type              = ENTITY_PLAYER_PROJECTILE;

  f32    ms            = 0.02;

  Entity player_entity = game_state.entities[game_state.player.entity];
  e->position          = game_state.entities[game_state.player.entity].position;
  e->velocity          = Vector2(cosf(player_entity.angle) * ms, sinf(player_entity.angle) * ms);
  e->angle             = player_entity.angle;
  e->r                 = 0.03f;
  e->render_data       = get_render_data_by_name("fireball");
  e->hp                = 1;
  if (player_entity.render_data->animation_controller)
  {
    set_animation(player_entity.render_data->animation_controller, "fireball", ticks);
  }

  return true;
}

struct Effect
{
  EntityRenderData render_data;
  Vector2          position;
  f32              angle;
  u32              effect_ends_at;
  void             (*update_effect)(Effect* effect, u32 tick);
};

struct EffectNode
{
  Effect      effect;
  EffectNode* next;
};
struct Effects
{
  PoolAllocator pool;
  EffectNode*   head;
};

Effects effects;

Vector3 project_vertex(Vector2 v, Mat44 m)
{
  Vector4 v4 = m.mul(Vector4(v.x, v.y, 0, 1.0));
  return v4.project();
}

bool ray_triangle_intersection(Vector2& p, Triangle* t, Mat44 view_proj, Vector2 ray)
{
  Triangle3D triangle;
  triangle.points[0] = project_vertex(t->points[0], view_proj);
  triangle.points[1] = project_vertex(t->points[1], view_proj);
  triangle.points[2] = project_vertex(t->points[2], view_proj);
  Vector3 eye        = Vector3(ray.x, ray.y, 0);
  Vector3 dir        = Vector3(eye.x, eye.y, eye.z - 1.0);
  float   u, v, w, time;
  if (IntersectSegmentTriangle(eye, dir, triangle, u, v, w, time))
  {
    p.x = t->points[0].x * u + t->points[1].x * v + t->points[2].x * w;
    p.y = t->points[0].y * u + t->points[1].y * v + t->points[2].y * w;
    return true;
  }
  p.x = t->points[0].x * u + t->points[1].x * v + t->points[2].x * w;
  p.y = t->points[0].y * u + t->points[1].y * v + t->points[2].y * w;
  return false;
}

bool find_cursor_position_on_map(Vector2& point, f32* mouse_position, Mat44 view_proj)
{
  Vector2 ray(mouse_position[0], mouse_position[1]);
  for (u32 i = 0; i < map.index_count - 2; i += 3)
  {
    Triangle t;
    t.points[0] = map.vertices[map.indices[i]];
    t.points[1] = map.vertices[map.indices[i + 1]];
    t.points[2] = map.vertices[map.indices[i + 2]];

    Vector2 p;
    if (ray_triangle_intersection(p, &t, view_proj, ray))
    {
      point = p;
      return true;
    }
    point = p;
  }
  return false;
}

bool use_ability_coc(Camera* camera, InputState* input, u32 ticks)
{
  // deal damage instantly
  // create and effect that last like a few frames
  Effect effect;
  effect.update_effect = 0;
  effect.render_data   = *get_render_data_by_name("cone of cold");

  // position is just the angle of the mouse, from our radius and outwards with half the length
  // the translation distance can be hardcoded
  Entity player_entity = game_state.entities[game_state.player.entity];

  effect.position      = player_entity.position;
  effect.position.x += cosf(player_entity.angle) * 0.25f;
  effect.position.y += sinf(player_entity.angle) * 0.25f;
  f32 scale = effect.render_data.scale;
  effect.position.x -= sinf(player_entity.angle) * scale * 0.5f;
  effect.position.y += cosf(player_entity.angle) * scale * 0.5f;

  effect.angle          = player_entity.angle;
  effect.effect_ends_at = ticks + 100;

  EffectNode* node      = (EffectNode*)effects.pool.alloc();
  node->next            = effects.head;
  effects.head          = node;

  node->effect          = effect;

  Model*  model         = get_model_by_name("model_coc");
  Vector2 vertices[model->vertex_count];
  for (u32 i = 0; i < model->vertex_count; i++)
  {
    vertices[i].x = effect.position.x + model->vertices[i].x * scale;
    vertices[i].y = effect.position.y + model->vertices[i].y * scale;
  }

  for (u32 i = 0; i < game_state.entity_count; i++)
  {
    Entity* entity = &game_state.entities[i];
    if (entity->visible && entity->type == ENTITY_ENEMY)
    {
      for (u32 i = 0; i < model->index_count; i += 3)
      {
        Triangle t;
        t.points[0].x = vertices[model->indices[i]].x;
        t.points[0].y = vertices[model->indices[i]].y;
        t.points[1].x = vertices[model->indices[i + 1]].x;
        t.points[1].y = vertices[model->indices[i + 1]].y;
        t.points[2].x = vertices[model->indices[i + 2]].x;
        t.points[2].y = vertices[model->indices[i + 2]].y;

        Vector2 cp    = closest_point_triangle(t, entity->position);
        f32     len   = cp.sub(entity->position).len();
        if (len < entity->r)
        {
          logger.info("Hit with coc!");
          entity->hp -= 1;
          break;
        }
      }
    }
  }

  if (player_entity.render_data->animation_controller)
  {
    set_animation(player_entity.render_data->animation_controller, "cone_of_cold", ticks);
  }

  return true;
}

bool use_ability_pillar_of_flame(Camera* camera, InputState* input, u32 ticks)
{

  Effect effect;
  effect.update_effect = 0;
  effect.render_data   = *get_render_data_by_name("pillar of flame");
  Entity  entity       = game_state.entities[game_state.player.entity];

  Mat44   view         = camera->get_view_matrix().mul(game_state.projection);

  Vector2 point        = {};
  if (!find_cursor_position_on_map(point, input->mouse_position, view))
  {
    return false;
  }

  CommandExplodePillarOfFlameData* data = sta_allocate_struct(CommandExplodePillarOfFlameData, 1);
  data->position                        = Vector2(point.x, point.y);
  effect.position                       = Vector2(point.x, point.y);

  effect.angle                          = entity.angle;
  effect.effect_ends_at                 = ticks + 500;

  EffectNode* node                      = (EffectNode*)effects.pool.alloc();
  node->next                            = effects.head;
  effects.head                          = node;

  node->effect                          = effect;

  // position
  add_command(CMD_EXPLODE_PILLAR, (void*)data, effect.effect_ends_at);
  if (entity.render_data->animation_controller)
  {
    set_animation(entity.render_data->animation_controller, "pillar_of_flame", ticks);
  }
  return true;
}

bool use_ability_melee(Camera* camera, InputState* input, u32 ticks)
{
  // deal damage instantly
  // create and effect that last like a few frames
  Effect effect;
  effect.update_effect = 0;
  effect.render_data   = *get_render_data_by_name("melee");

  // position is just the angle of the mouse, from our radius and outwards with half the length
  // the translation distance can be hardcoded
  Entity player_entity = game_state.entities[game_state.player.entity];

  effect.position      = player_entity.position;
  effect.position.x += cosf(player_entity.angle) * 0.05f;
  effect.position.y += sinf(player_entity.angle) * 0.05f;
  f32 scale = effect.render_data.scale;
  effect.position.x -= sinf(player_entity.angle) * scale * 0.5f;
  effect.position.y += cosf(player_entity.angle) * scale * 0.5f;

  effect.angle          = player_entity.angle;
  effect.effect_ends_at = ticks + 100;

  EffectNode* node      = (EffectNode*)effects.pool.alloc();
  node->next            = effects.head;
  effects.head          = node;

  node->effect          = effect;

  Model*  model         = get_model_by_name("model_coc");
  Vector2 vertices[model->vertex_count];
  for (u32 i = 0; i < model->vertex_count; i++)
  {
    vertices[i].x = effect.position.x + model->vertices[i].x * scale;
    vertices[i].y = effect.position.y + model->vertices[i].y * scale;
  }

  for (u32 i = 0; i < game_state.entity_count; i++)
  {
    Entity* entity = &game_state.entities[i];
    if (entity->visible && entity->type == ENTITY_ENEMY)
    {
      for (u32 i = 0; i < model->index_count; i += 3)
      {
        Triangle t;
        t.points[0].x = vertices[model->indices[i]].x;
        t.points[0].y = vertices[model->indices[i]].y;
        t.points[1].x = vertices[model->indices[i + 1]].x;
        t.points[1].y = vertices[model->indices[i + 1]].y;
        t.points[2].x = vertices[model->indices[i + 2]].x;
        t.points[2].y = vertices[model->indices[i + 2]].y;

        Vector2 cp    = closest_point_triangle(t, entity->position);
        f32     len   = cp.sub(entity->position).len();
        if (len < entity->r)
        {
          logger.info("Hit with coc!");
          entity->hp -= 1;
          break;
        }
      }
    }
    AnimationController* controller = player_entity.render_data->animation_controller;
    if (controller)
    {
      set_animation(controller, "melee", ticks);
    }
  }

  return true;
}
bool use_ability_charge(Camera* camera, InputState* input, u32 ticks)
{
  game_state.player.can_move = false;
  Entity* p                  = &game_state.entities[game_state.player.entity];
  p->velocity.x              = cosf(p->angle) * 0.1f;
  p->velocity.y              = sinf(p->angle) * 0.1f;
  add_command(CMD_STOP_CHARGE, 0, ticks + 150);
  AnimationController* controller = p->render_data->animation_controller;
  if (controller)
  {
    set_animation(controller, "charge", ticks);
  }
  return true;
}
bool use_ability_thunderclap(Camera* camera, InputState* input, u32 ticks)
{

  Effect effect;
  effect.update_effect  = 0;
  effect.render_data    = *get_render_data_by_name("pillar of flame");
  Entity entity         = game_state.entities[game_state.player.entity];

  effect.position       = entity.position;

  effect.angle          = entity.angle;
  effect.effect_ends_at = ticks + 100;

  EffectNode* node      = (EffectNode*)effects.pool.alloc();
  node->next            = effects.head;
  effects.head          = node;

  node->effect          = effect;

  f32     scale         = effect.render_data.scale;
  Model*  model         = get_model_by_name("model_coc");
  Vector2 vertices[model->vertex_count];
  for (u32 i = 0; i < model->vertex_count; i++)
  {
    vertices[i].x = effect.position.x + model->vertices[i].x * scale;
    vertices[i].y = effect.position.y + model->vertices[i].y * scale;
  }

  for (u32 i = 0; i < game_state.entity_count; i++)
  {
    Entity* entity = &game_state.entities[i];
    if (entity->visible && entity->type == ENTITY_ENEMY)
    {
      for (u32 i = 0; i < model->index_count; i += 3)
      {
        Triangle t;
        t.points[0].x = vertices[model->indices[i]].x;
        t.points[0].y = vertices[model->indices[i]].y;
        t.points[1].x = vertices[model->indices[i + 1]].x;
        t.points[1].y = vertices[model->indices[i + 1]].y;
        t.points[2].x = vertices[model->indices[i + 2]].x;
        t.points[2].y = vertices[model->indices[i + 2]].y;

        Vector2 cp    = closest_point_triangle(t, entity->position);
        f32     len   = cp.sub(entity->position).len();
        if (len < entity->r)
        {
          logger.info("Hit with coc!");
          entity->hp -= 1;
          break;
        }
      }
    }
    AnimationController* controller = entity->render_data->animation_controller;
    if (controller)
    {
      set_animation(controller, "thunder_clap", ticks);
    }
  }

  return true;
}
bool use_ability_slam(Camera* camera, InputState* input, u32 ticks)
{
  return true;
}

bool (*use_ability_function_ptrs[])(Camera* camera, InputState* input, u32 ticks) = {
    use_ability_fireball,        //
    use_ability_blink,           //
    use_ability_pillar_of_flame, //
    use_ability_coc,             //
    use_ability_melee,           //
    use_ability_charge,          //
    use_ability_thunderclap,     //
    use_ability_slam,            //
};
Ability* abilities;
u32      ability_count;

Ability  get_ability_by_name(const char* name)
{
  for (u32 i = 0; i < ability_count; i++)
  {
    if (compare_strings(abilities[i].name, name))
    {
      return abilities[i];
    }
  }
  logger.error("Didn't find ability '%s'", name);
  assert(!"Didn't find ability!");
}

void read_abilities(Ability* abilities)
{
  abilities[0] = get_ability_by_name("Fireball");
  abilities[1] = get_ability_by_name("Blink");
  abilities[2] = get_ability_by_name("Pillar of flame");
}

void handle_enemy_movement(Enemy* enemy, Vector2 target_position)
{
  const static f32 ms         = 0.005f;
  Entity*          entity     = &game_state.entities[enemy->entity];
  u32              tile_count = get_tile_count_per_row();

  // enemy->path.path_count = 0;
  // find_path(&enemy->path, target_position, entity->position);

  Path path = enemy->path;
  if (path.path_count == 1)
  {
    return;
  }

  u32     path_idx           = 1;
  Vector2 prev_position      = entity->position;
  Vector2 curr               = entity->position;
  f32     movement_remaining = ms;
  while (true)
  {
    f32 x = tile_position_to_game(path.path[path_idx] >> 8);
    f32 y = tile_position_to_game(path.path[path_idx] & 0xFF);
    path_idx++;
    if (compare_float(x, curr.x) && compare_float(y, curr.y))
    {
      continue;
    }
    Vector2 point(x, y);

    // distance from curr to point
    f32 moved = (ABS(point.x - curr.x) + ABS(point.y - curr.y)) / entity->r;
    if (moved > movement_remaining)
    {
      Vector2 velocity(point.x - curr.x, point.y - curr.y);
      curr = point;
      velocity.normalize();
      velocity.scale(movement_remaining);
      entity->velocity.x = velocity.x;
      entity->velocity.y = velocity.y;
      break;
    }
    // convert current position to curr
    movement_remaining -= moved;
    curr = point;
    if (path_idx >= path.path_count)
    {
      break;
    }
  }
  Vector2 v(curr.x - prev_position.x, curr.y - prev_position.y);
  entity->angle = atan2f(v.y, v.x);
}

inline bool point_in_sphere(Sphere sphere, Vector2 p)
{

  return sphere.position.sub(p).len() < sphere.r;
}

bool player_is_visible(f32& angle, Vector2 position)
{

  const f32 step_size     = 0.005f;
  Entity    player_entity = game_state.entities[game_state.player.entity];
  // ToDo this also just checks for the origin of the player and not the hitbox
  Vector2 direction = player_entity.position.sub(position);

  Vector2 orig_pos  = position;

  Sphere  player_sphere;
  player_sphere.position = player_entity.position;
  player_sphere.r        = player_entity.r;
  while (true)
  {
    position.x += direction.x * step_size;
    position.y += direction.y * step_size;
    Vector2 closest_point = {};
    // ToDo this needs to change after geometry change
    if (is_out_of_map_bounds(closest_point, position, player_entity.r) || collides_with_static_geometry(closest_point, position, player_entity.r))
    {
      return false;
    }
    if (point_in_sphere(player_sphere, position))
    {
      f32 x = position.x - orig_pos.x;
      f32 y = position.y - orig_pos.y;
      angle = atan2(y, x);
      return true;
    }
  }

  return true;
}

void update_enemies(Wave* wave, u32 tick_difference, u32 tick)
{

  Sphere player_sphere;

  player_sphere.r        = game_state.entities[game_state.player.entity].r;
  player_sphere.position = game_state.entities[game_state.player.entity].position;

  // check if we spawn

  Vector2 player_position = game_state.entities[game_state.player.entity].position;
  u32     spawn_count     = __builtin_popcountll(wave->spawn_count);
  assert(spawn_count <= wave->enemy_count && "More spawning then enemies?");
  for (u32 i = 0; i < spawn_count; i++)
  {
    Enemy*  enemy  = &wave->enemies[i];
    Entity* entity = &game_state.entities[enemy->entity];
    if (entity->visible)
    {
      if (enemy->can_move)
      {
        handle_enemy_movement(enemy, player_position);
      }
      else
      {
        entity->velocity.x = 0;
        entity->velocity.y = 0;
      }
      Sphere enemy_sphere;
      enemy_sphere.r        = entity->r;
      enemy_sphere.position = entity->position;

      switch (enemy->type)
      {
      case ENEMY_MELEE:
      {
        if (tick >= enemy->cooldown_timer && sphere_sphere_collision(player_sphere, enemy_sphere))
        {

          if (tick >= game_state.player.can_take_damage_tick && !game_state.god)
          {
            game_state.player.can_take_damage_tick = tick + game_state.player.damage_taken_cd;
            // entities[player->entity].hp -= 1;
            // logger.info("Took damage, hp: %d %d %d", game_state.player.entity, enemy->entity, i);
          }
          if (entity->render_data->animation_controller)
          {
            set_animation(entity->render_data->animation_controller, "melee", tick);
          }
          enemy->cooldown_timer = tick + enemy->cooldown;
        }
        break;
      }
      case ENEMY_RANGED:
      {
        // check cd
        if (tick >= enemy->cooldown_timer)
        {

          // check if visible
          f32 angle = 0.0f;
          if (player_is_visible(angle, entity->position))
          {

            enemy->can_move                               = false;

            CommandLetRangedMoveAfterShooting* arrow_data = sta_allocate_struct(CommandLetRangedMoveAfterShooting, 1);
            arrow_data->enemy                             = enemy;
            if (entity->render_data->animation_controller)
            {
              set_animation(entity->render_data->animation_controller, "shoot", tick);
            }
            add_command(CMD_LET_RANGED_MOVE_AFTER_SHOOTING, (void*)arrow_data, tick + 300);
            u32     entity_index  = get_new_entity();
            Entity* entity        = &game_state.entities[enemy->entity];
            Entity* e             = &game_state.entities[entity_index];
            e->type               = ENTITY_ENEMY_PROJECTILE;
            e->visible            = true;

            f32 ms                = 0.01;

            e->position           = entity->position;
            e->velocity           = Vector2(cosf(angle) * ms, sinf(angle) * ms);
            e->angle              = entity->angle;
            e->r                  = 0.03f;
            e->render_data        = get_render_data_by_name("arrow");
            e->hp                 = 1;

            enemy->cooldown_timer = tick + enemy->cooldown;
          }
        }
      }
      }
    }
  }
}

void handle_collision(Entity* e1, Entity* e2, Wave* wave)
{
  if ((e1->type == ENTITY_ENEMY && e2->type == ENTITY_PLAYER_PROJECTILE) || (e1->type == ENTITY_PLAYER_PROJECTILE && e2->type == ENTITY_ENEMY))
  {
    game_state.score += 100;
    e1->hp = 0;
    e2->hp = 0;
    wave->enemies_alive -= 1;
  }
  // ToDo physics sim?
  if (e1->type == ENTITY_ENEMY && e2->type == ENTITY_ENEMY)
  {
  }
  if ((e1->type == ENTITY_ENEMY && e2->type == ENTITY_PLAYER) || (e1->type == ENTITY_PLAYER && e2->type == ENTITY_ENEMY))
  {
    if (!game_state.player.can_move)
    {
      game_state.player.can_move = true;
      if (e1->type == ENTITY_ENEMY)
      {
        e1->hp -= 1;
      }
      else if (e2->type == ENTITY_ENEMY)
      {
        e2->hp -= 1;
      }
    }
  }
  if ((e1->type == ENTITY_PLAYER && e2->type == ENTITY_ENEMY_PROJECTILE) || (e1->type == ENTITY_ENEMY_PROJECTILE && e2->type == ENTITY_PLAYER))
  {
    e2->hp = 0;
  }
  if (e1->hp == 0)
  {
    e1->visible = false;
  }
  if (e2->hp == 0)
  {
    e2->visible = false;
  }
}

void update_entities(Entity* entities, u32 entity_count, Wave* wave, u32 tick_difference)
{

  for (u32 i = 0; i < entity_count; i++)
  {

    Entity* entity = &entities[i];
    if (entity->visible)
    {
      f32 diff = (f32)tick_difference / 16.0f;
      entity->position.x += entity->velocity.x * diff;
      entity->position.y += entity->velocity.y * diff;
      Vector2 closest_point = {};
      // // ToDo very bug prone this yes
      if (collides_with_static_geometry(closest_point, entity->position, entity->r))
      {
        if (entity->type == ENTITY_PLAYER_PROJECTILE || entity->type == ENTITY_ENEMY_PROJECTILE)
        {
          entity->hp      = 0;
          entity->visible = false;
        }
        if (entity->type == ENTITY_PLAYER && !game_state.player.can_move)
        {
          game_state.player.can_move = true;
        }

        entity->position.x -= entity->velocity.x * diff;
        entity->position.y -= entity->velocity.y * diff;
      }

      if (is_out_of_map_bounds(closest_point, entity->position, entity->r))
      {
        if (entity->type == ENTITY_PLAYER_PROJECTILE || entity->type == ENTITY_ENEMY_PROJECTILE)
        {
          entity->hp      = 0;
          entity->visible = false;
        }
        if (entity->type == ENTITY_PLAYER && !game_state.player.can_move)
        {
          game_state.player.can_move = true;
        }
        entity->position = closest_point;
      }
    }
  }
  for (u32 i = 0; i < entity_count - 1; i++)
  {
    Entity* e1 = &entities[i];
    if (e1->visible == false)
    {
      continue;
    }
    Sphere e1_sphere;
    e1_sphere.r        = e1->r;
    e1_sphere.position = e1->position;
    for (u32 j = i + 1; j < entity_count; j++)
    {
      Entity* e2 = &entities[j];
      if (e2->visible == false)
      {
        continue;
      }
      Sphere e2_sphere;
      e2_sphere.r        = e2->r;
      e2_sphere.position = e2->position;
      if (sphere_sphere_collision(e1_sphere, e2_sphere))
      {
        handle_collision(e1, e2, wave);
        if (e1->visible == false)
        {
          break;
        }
      }
    }
  }
}

struct EnemyData
{
  EnemyType   type;
  u32         hp;
  f32         radius;
  u32         cooldown;
  const char* render_data_name;
};

EnemyData* enemy_data;
u32        enemy_data_count;

EnemyData  get_enemy_data_from_type(EnemyType type)
{
  for (u32 i = 0; i < enemy_data_count; i++)
  {
    if (enemy_data[i].type == type)
    {
      return enemy_data[i];
    }
  }
  logger.error("Couldn't find enemy with this type! %d", type);
  assert(!"Couldn't find enemy with this type!");
}

bool load_enemies_from_file(const char* filename)
{
  Buffer buffer = {};
  if (!sta_read_file(&buffer, filename))
  {
    return false;
  }
  Json json = {};
  if (!sta_json_deserialize_from_string(&buffer, &json))
  {
    return false;
  };

  assert(json.headType == JSON_OBJECT && "Expected head to be object");
  JsonObject* head  = &json.obj;

  u32         count = head->size;
  enemy_data_count  = count;
  enemy_data        = sta_allocate_struct(EnemyData, count);

  for (u32 i = 0; i < count; i++)
  {
    EnemyData*  data       = &enemy_data[i];
    JsonObject* json_data  = head->values[i].obj;

    data->type             = (EnemyType)json_data->lookup_value("type")->number;
    data->hp               = json_data->lookup_value("hp")->number;
    data->radius           = json_data->lookup_value("radius")->number;
    data->cooldown         = json_data->lookup_value("cooldown")->number;

    data->render_data_name = head->keys[i];
    logger.info("Found enemy %d: %d %f %s", data->type, data->hp, data->radius, data->render_data_name);
  }

  return true;
}

bool load_wave_from_file(Wave* wave, const char* filename)
{
  Buffer      buffer = {};
  StringArray lines  = {};
  if (!sta_read_file(&buffer, filename))
  {
    return false;
  }
  split_buffer_by_newline(&lines, &buffer);

  assert(lines.count > 1 && "Only one line in wave file?");
  wave->enemy_count = parse_int_from_string(lines.strings[0]);
  if (wave->enemy_count * 2 + 1 != lines.count)
  {
    u32 ec = wave->enemy_count;
    if (lines.count < wave->enemy_count * 2 + 1)
    {
      ec = (lines.count - 1) / 2;
    }

    logger.error("Mismatch in wave file '%s', expected %d enemies (%d lines) but got %d lines, grabbing %d enemies", filename, wave->enemy_count, wave->enemy_count * 2 + 1, lines.count, ec);
    wave->enemy_count = ec;
  }
  wave->spawn_count   = 0;
  wave->enemies_alive = 0;
  wave->spawn_times   = sta_allocate_struct(u32, wave->enemy_count);
  wave->enemies       = sta_allocate_struct(Enemy, wave->enemy_count);
  for (u32 i = 0, string_index = 1; i < wave->enemy_count; i++)
  {

    Enemy* enemy                = &wave->enemies[i];
    enemy->can_move             = true;
    enemy->type                 = (EnemyType)parse_int_from_string(lines.strings[string_index++]);
    EnemyData enemy_data        = get_enemy_data_from_type(enemy->type);
    enemy->initial_hp           = enemy_data.hp;
    enemy->cooldown             = enemy_data.cooldown;
    enemy->cooldown_timer       = 0;

    wave->spawn_times[i]        = parse_int_from_string(lines.strings[string_index++]);
    u32 entity_idx              = get_new_entity();
    enemy->entity               = entity_idx;

    Entity* entity              = &game_state.entities[entity_idx];

    entity->visible             = true;
    entity->render_data         = get_render_data_by_name(enemy_data.render_data_name);
    entity->type                = ENTITY_ENEMY;
    entity->angle               = 0.0f;
    entity->hp                  = 0;
    entity->r                   = enemy_data.radius;
    entity->velocity            = Vector2(0, 0);

    u32 tile_count              = get_tile_count_per_row();
    enemy->path.path            = sta_allocate_struct(u16, tile_count * tile_count);

    CommandSpawnEnemyData* data = sta_allocate_struct(CommandSpawnEnemyData, 1);
    data->enemy_index           = i;
    data->wave                  = wave;
    add_command(CMD_SPAWN_ENEMY, (void*)data, wave->spawn_times[i]);
  }
  logger.info("Read wave from '%s', got %d enemies", filename, wave->enemy_count);

  return true;
}

bool load_entity_render_data_from_file(const char* file_location)
{

  Buffer buffer = {};
  if (!sta_read_file(&buffer, file_location))
  {
    return false;
  }
  Json json = {};
  if (!sta_json_deserialize_from_string(&buffer, &json))
  {
    return false;
  };
  assert(json.headType == JSON_OBJECT && "Expected head to be object");
  JsonObject* head             = &json.obj;

  u32         count            = head->size;

  game_state.render_data       = sta_allocate_struct(EntityRenderData, count);
  game_state.render_data_count = count;
  for (u32 i = 0; i < count; i++)
  {
    EntityRenderData* data      = &game_state.render_data[i];
    data->name                  = head->keys[i];
    JsonObject* render_data_obj = head->values[i].obj;
    if (render_data_obj->lookup_value("animation"))
    {
      Model* model = get_model_by_name(render_data_obj->lookup_value("animation")->string);
      assert(model->animation_data && "No animation data for the model!");
      data->animation_controller = animation_controller_create(model->animation_data);
    }
    else
    {
      data->animation_controller = 0;
    }

    const char* texture         = render_data_obj->lookup_value("texture")->string;
    const char* shader          = render_data_obj->lookup_value("shader")->string;
    f32         scale           = render_data_obj->lookup_value("scale")->number;
    const char* buffer_id       = render_data_obj->lookup_value("buffer")->string;
    JsonValue*  rotation_x_json = render_data_obj->lookup_value("rotation_x");
    if (rotation_x_json)
    {
      data->rotation_x = rotation_x_json->number;
    }
    else
    {
      data->rotation_x = 0;
    }
    data->texture   = game_state.renderer.get_texture(texture);
    data->shader    = game_state.renderer.get_shader_by_name(shader);
    data->scale     = scale;
    data->buffer_id = get_buffer_by_name(buffer_id);
    logger.info("Loaded render data for '%s': texture: %s %d, shader: %s %d, scale: %f,  buffer: %s %d", data->name, texture, data->texture, shader, data->shader, scale, buffer_id, data->buffer_id);
  }

  return true;
}

static f32 p = PI;

Hero*      heroes;
u32        hero_count;

Hero       get_hero_by_name(const char* name)
{
  for (u32 i = 0; i < hero_count; i++)
  {
    if (compare_strings(name, heroes[i].name))
    {
      return heroes[i];
    }
  }
  logger.error("Couldn't find hero! %s", name);
  assert(!"Couldn't find hero!");
}

bool load_hero_from_file(const char* file_location)
{
  Buffer buffer = {};
  if (!sta_read_file(&buffer, file_location))
  {
    return false;
  }
  Json json = {};
  if (!sta_json_deserialize_from_string(&buffer, &json))
  {
    return false;
  };
  assert(json.headType == JSON_OBJECT && "Expected head to be object");
  JsonObject* head  = &json.obj;

  u32         count = head->size;
  hero_count        = count;
  heroes            = sta_allocate_struct(Hero, count);

  logger.info("Found %d heroes", count);
  for (u32 i = 0; i < count; i++)
  {
    char*      name          = head->keys[i];

    JsonArray* ability_array = head->values[i].obj->lookup_value("abilities")->arr;
    u32        ability_count = ability_array->arraySize;
    Hero*      hero          = &heroes[i];
    assert(ability_count < ArrayCount(hero->abilities) && "Too many abilities!!");
    for (u32 j = 0; j < ability_count; j++)
    {
      hero->abilities[j] = get_ability_by_name(ability_array->values[j].string);
    }
    hero->damage_taken_cd      = 0;
    hero->can_take_damage_tick = 0;
    hero->name                 = name;

    JsonObject* hero_obj       = head->values[i].obj;
    u32         entity_index   = get_new_entity();
    Entity*     entity         = &game_state.entities[entity_index];
    hero->entity               = entity_index;
    entity->type               = ENTITY_PLAYER;
    entity->render_data        = get_render_data_by_name("player");
    entity->position           = Vector2(0.5, 0.5);
    entity->velocity           = Vector2(0, 0);
    entity->r                  = hero_obj->lookup_value("radius")->number;
    entity->hp                 = hero_obj->lookup_value("hp")->number;
    entity->visible            = false;
  }
  return true;
}
bool load_ability_from_file(const char* file_location)
{
  Buffer buffer = {};
  if (!sta_read_file(&buffer, file_location))
  {
    return false;
  }
  Json json = {};
  if (!sta_json_deserialize_from_string(&buffer, &json))
  {
    return false;
  };

  assert(json.headType == JSON_OBJECT && "Expected head to be object");
  JsonObject* head  = &json.obj;
  u32         count = head->size;
  abilities         = sta_allocate_struct(Ability, count);
  ability_count     = count;

  logger.info("Found %d abilities", count);
  for (u32 i = 0; i < count; i++)
  {
    char* name                  = head->keys[i];
    u32   use_ability_index     = head->values[i].obj->lookup_value("use_ptr_idx")->number;
    u32   cooldown              = head->values[i].obj->lookup_value("cooldown")->number;
    abilities[i].name           = name;
    abilities[i].cooldown_ticks = cooldown;
    abilities[i].use_ability    = use_ability_function_ptrs[use_ability_index];
    abilities[i].cooldown       = 0;
  }
  return true;
}

bool load_data()
{

  const static char* enemy_data_location = "./data/formats/enemy.json";
  if (!load_enemies_from_file(enemy_data_location))
  {
    logger.error("Failed to read models from '%s'", enemy_data_location);
    return false;
  }

  const static char* model_locations = "./data/formats/models.json";
  if (!load_models_from_files(model_locations))
  {
    logger.error("Failed to read models from '%s'", model_locations);
    return false;
  }

  const static char* texture_locations = "./data/formats/textures.json";
  if (!game_state.renderer.load_textures_from_files(texture_locations))
  {
    logger.error("Failed to read textures from '%s'", texture_locations);
    return false;
  }

  const static char* shader_locations = "./data/formats/shader.json";
  if (!game_state.renderer.load_shaders_from_files(shader_locations))
  {
    logger.error("Failed to read shaders from '%s'", shader_locations);
    return false;
  }

  const static char* buffer_locations = "./data/formats/buffers.json";
  if (!load_buffers_from_files(buffer_locations))
  {
    logger.error("Failed to read buffers from '%s'", buffer_locations);
    return false;
  }

  const static char* noise_locations = "./data/textures/noise01.tga";
  if (!sta_targa_read_from_file_rgba(&noise, noise_locations))
  {
    logger.error("Failed to read noise from '%s'", noise_locations);
    return false;
  }

  const static char* render_data_location = "./data/formats/render_data.json";
  if (!load_entity_render_data_from_file(render_data_location))
  {
    logger.error("Failed to read render data from '%s'", render_data_location);
    return false;
  }
  const static char* map_data_location = "./data/maps/map01.json";
  if (!load_map_from_file(map_data_location))
  {
    logger.error("Failed to read map data from '%s'", map_data_location);
    return false;
  }
  const static char* ability_data_location = "./data/formats/abilities.json";
  if (!load_ability_from_file(ability_data_location))
  {
    logger.error("Failed to read ability data from '%s'", ability_data_location);
    return false;
  }
  const static char* hero_data_location = "./data/formats/hero.json";
  if (!load_hero_from_file(hero_data_location))
  {
    logger.error("Failed to read hero data from '%s'", hero_data_location);
    return false;
  }
  return true;
}

void init_player(Hero* player)
{

  *player                                     = get_hero_by_name("Mage");
  game_state.entities[player->entity].visible = true;
  player->can_move                            = true;
  EntityRenderData rd                         = *game_state.entities[player->entity].render_data;
  logger.info("Inited player %d", rd.texture);
}

void render_console(Hero* player, InputState* input_state, char* console_buf, u32 console_buf_size, Wave* wave, u32& game_running_ticks)
{

  if (input_state->is_key_released(ASCII_RETURN))
  {
    logger.info("Released buffer: %s", console_buf);
    if (compare_strings("restart", console_buf))
    {
      logger.info("Restarting wave");
      wave->enemies_alive = 0;
      u32 spawn_count     = __builtin_popcountll(wave->spawn_count);
      for (u32 i = 0; i < spawn_count; i++)
      {
        Entity* e = &game_state.entities[wave->enemies[i].entity];
        e->hp     = 0;
      }
      wave->spawn_count                      = 0;
      game_running_ticks                     = 0;
      game_state.entities[player->entity].hp = 3;
      game_state.score                       = 0;
    }
    else if (compare_strings("vsync", console_buf))
    {
      game_state.renderer.toggle_vsync();
      logger.info("toggled vsync to %d!\n", game_state.renderer.vsync);
    }
    else if (compare_strings("god", console_buf))
    {
      game_state.god = !game_state.god;
      logger.info("godmode toggled to %d!\n", game_state.god);
    }
    else if (compare_strings("debug", console_buf))
    {
#if DEBUG
#undef DEBUG
#else
#define DEBUG
#endif
      logger.info("Debug on");
    }
    else if (compare_strings("reload", console_buf))
    {
      game_state.renderer.reload_shaders();
    }
    memset(console_buf, 0, console_buf_size);
  }
  ImGui::Begin("Console!", 0, ImGuiWindowFlags_NoDecoration);
  if (ImGui::InputText("Console input", console_buf, console_buf_size))
  {
  }

  ImGui::End();
}

void update_effects(u32 ticks)
{
  EffectNode* node = effects.head;
  EffectNode* prev = 0;
  while (node)
  {

    Effect effect = node->effect;
    if (ticks > effect.effect_ends_at)
    {
      if (prev)
      {
        prev->next = node->next;
      }
      else
      {
        effects.head = 0;
      }
      EffectNode* next = node->next;
      effects.pool.free((u64)node);
      node = next;
      if (!node)
      {
        break;
      }
    }

    node = node->next;
  }
}

void update(Wave* wave, Camera& camera, InputState* input_state, u32 game_running_ticks, u32 tick_difference)
{

  handle_abilities(camera, input_state, game_running_ticks);
  handle_player_movement(camera, input_state, game_running_ticks);
  run_commands(game_running_ticks);
  update_entities(game_state.entities, game_state.entity_count, wave, tick_difference);
  update_effects(game_running_ticks);
  update_enemies(wave, tick_difference, game_running_ticks);

  update_animations(game_running_ticks);
}

inline bool handle_wave_over(Wave* wave)
{
  u32 spawn_count = __builtin_popcountll(wave->spawn_count);
  return spawn_count == wave->enemy_count && wave->enemies_alive == 0;
}

void debug_render_depth_texture_cube(u32 texture)
{

  static u32    vao = 0, vbo = 0;
  static f32    x = 0;
  static Shader q_shader;

  if (vao == 0)
  {
    ShaderType  types[2]     = {SHADER_TYPE_VERT, SHADER_TYPE_FRAG};
    const char* locations[2] = {
        "./shaders/skybox.vert",
        "./shaders/skybox.frag",
    };
    q_shader               = Shader(types, locations, 2, "skybox");
    float skyboxVertices[] = {// positions
                              -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
                              -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f, 1.0f,
                              1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f,
                              1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f, -1.0f,
                              1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

    sta_glGenVertexArrays(1, &vao);
    sta_glGenBuffers(1, &vbo);
    sta_glBindVertexArray(vao);
    sta_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    sta_glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * ArrayCount(skyboxVertices), skyboxVertices, GL_DYNAMIC_DRAW);

    sta_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(f32) * 3, (void*)(sizeof(f32) * 0));
    sta_glEnableVertexAttribArray(0);

    sta_glBindVertexArray(0);
  }
  q_shader.use();
  game_state.renderer.bind_cube_texture(q_shader, "skybox", texture);
  x -= 0.5;

  Mat44 model = Mat44::identity();
  model       = model.scale(0.5f).rotate_x(x).rotate_y(x).rotate_z(x);
  q_shader.set_mat4("view", model);
  q_shader.set_mat4("projection", Mat44::identity());
  sta_glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, 36);
}
void debug_render_depth_texture(u32 texture)
{
  Shader q_shader = *game_state.renderer.get_shader_by_index(game_state.renderer.get_shader_by_name("quad"));
  q_shader.use();

  game_state.renderer.bind_cube_texture(q_shader, "texture1", texture);
  static u32 vao = 0, vbo = 0, ebo = 0;

  if (vao == 0)
  {
    sta_glGenVertexArrays(1, &vao);
    sta_glGenBuffers(1, &vbo);
    sta_glGenBuffers(1, &ebo);
    sta_glBindVertexArray(vao);
    f32 tot[20] = {
        0.8f, .8f,  0, 1.0f, 1.0f, //
        .8f,  -.8,  0, 1.0f, 0.0f, //
        -.8f, -.8f, 0, 0.0f, 0.0f, //
        -.8f, .8,   0, 0.0f, 1.0f, //
    };
    u32 indices[6] = {1, 3, 0, 1, 2, 3};
    sta_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    sta_glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * ArrayCount(tot), tot, GL_DYNAMIC_DRAW);
    sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * ArrayCount(indices), indices, GL_DYNAMIC_DRAW);

    sta_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(f32) * 5, (void*)(sizeof(f32) * 0));
    sta_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(f32) * 5, (void*)(sizeof(f32) * 3));
    sta_glEnableVertexAttribArray(0);
    sta_glEnableVertexAttribArray(1);

    sta_glBindVertexArray(0);
  }

  Mat44 model = Mat44::identity();
  model       = model.scale(0.1f);
  q_shader.set_mat4("model", model);
  sta_glBindVertexArray(vao);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void render_effects(u32 ticks)
{
  EffectNode* node = effects.head;
  EffectNode* prev = 0;
  while (node)
  {

    Effect effect = node->effect;
    if (ticks > effect.effect_ends_at)
    {
      if (prev)
      {
        prev->next = node->next;
      }
      else
      {
        effects.head = 0;
      }
      EffectNode* next = node->next;
      effects.pool.free((u64)node);
      node = next;
      if (!node)
      {
        break;
      }
    }
    EntityRenderData* render_data = &effect.render_data;
    Shader*           shader      = game_state.renderer.get_shader_by_index(render_data->shader);
    shader->use();
    Mat44 m = render_data->get_model_matrix();
    m       = m.rotate_z(RADIANS_TO_DEGREES(effect.angle) + 90);
    m       = m.translate(Vector3(effect.position.x, effect.position.y, 0.0f));

    game_state.renderer.bind_texture(*shader, "texture1", render_data->texture);
    shader->set_mat4("model", m);
    shader->set_mat4("view", game_state.camera.get_view_matrix());
    shader->set_mat4("projection", game_state.projection);
    game_state.renderer.render_buffer(render_data->buffer_id);

    node = node->next;
  }
}

void render_static_geometry()
{
  // load the static geometry that exists outside of the plane/map
  for (u32 i = 0; i < map.static_geometry.count; i++)
  {

    EntityRenderData* render_data = &map.static_geometry.render_data[i];
    Vector3           cube_pos    = map.static_geometry.position[i];

    Mat44             m           = Mat44::identity();
    m                             = m.scale(render_data->scale).translate(cube_pos);

    Shader* shader                = game_state.renderer.get_shader_by_index(render_data->shader);
    game_state.renderer.bind_texture(*shader, "texture1", render_data->texture);
    game_state.renderer.bind_texture(*shader, "shadow_map", game_state.renderer.depth_texture);
    shader->set_mat4("model", m);
    shader->set_mat4("view", game_state.camera.get_view_matrix());
    shader->set_vec3("viewPos", game_state.camera.translation);
    shader->set_mat4("projection", game_state.projection);
    shader->set_mat4("light_space_matrix", game_state.renderer.light_space_matrix);
    game_state.renderer.render_buffer(render_data->buffer_id);
  }
}

void render_paths(Wave* wave)
{
  Mat44 m           = Mat44::identity();
  u32   spawn_count = __builtin_popcountll(wave->spawn_count);
  for (u32 i = 0; i < spawn_count; i++)
  {
    Path path = wave->enemies[i].path;
    for (u32 j = 0; j < path.path_count - 1; j++)
    {
      f32 x1, y1, x2, y2;
      x1 = tile_position_to_game(path.path[j] >> 8);
      y1 = tile_position_to_game(path.path[j] & 0xFF);
      x2 = tile_position_to_game(path.path[j + 1] >> 8);
      y2 = tile_position_to_game(path.path[j + 1] & 0xFF);

      game_state.renderer.draw_circle(game_state.entities[wave->enemies[i].entity].position, game_state.entities[wave->enemies[i].entity].r, 1, RED, m, m);
      game_state.renderer.draw_line(x1, y1, x2, y2, 1, BLUE);
    }
  }
}

void debug_render_map_grid()
{
  u32 tile_count = get_tile_count_per_row();
  f32 tile_size  = get_tile_size();
  for (u32 x = 0; x < tile_count; x++)
  {
    f32 x0 = tile_position_to_game2(x);
    f32 x1 = x0 + tile_size;
    for (u32 y = 0; y < tile_count; y++)
    {
      if (is_tile_in_map(x, y, tile_size))
      {
        f32 y0 = tile_position_to_game2(y);
        f32 y1 = y0 + tile_size;
        game_state.renderer.draw_line(x0, y0, x0, y1, 1, BLUE);
        game_state.renderer.draw_line(x1, y0, x1, y1, 1, BLUE);
        game_state.renderer.draw_line(x0, y1, x1, y1, 1, BLUE);
        game_state.renderer.draw_line(x0, y0, x1, y0, 1, BLUE);
      }
    }
  }
}

void debug_render_everything(Wave* wave)
{

  Mat44 m = Mat44::identity();
  debug_render_map_grid();
  render_paths(wave);
  game_state.renderer.draw_circle(game_state.entities[game_state.player.entity].position, game_state.entities[game_state.player.entity].r, 1, YELLOW, m, m);
}

void animation_test_bed(InputState input_state)
{
  Renderer*         renderer    = &game_state.renderer;
  EntityRenderData* render_data = game_state.entities[game_state.player.entity].render_data;
  u32               joint_count = render_data->animation_controller->animation_data->skeleton.joint_count;
  u32               buffer      = render_data->buffer_id;
  Shader*           shader      = renderer->get_shader_by_index(renderer->get_shader_by_name("animation2"));
  shader->use();
  renderer->bind_texture(*shader, "texture1", render_data->texture);
  Mat44 jointTransforms[joint_count];
  for (u32 i = 0; i < joint_count; i++)
  {
    jointTransforms[i] = Mat44::identity();
  }

  while (true)
  {

    u32 ticks = SDL_GetTicks();
    input_state.update();
    if (input_state.should_quit())
    {
      exit(1);
    }
    handle_abilities(game_state.camera, &input_state, ticks);
    handle_player_movement(game_state.camera, &input_state, ticks);
    update_animations(ticks);

    renderer->clear_framebuffer();
    shader->set_mat4("jointTransforms", render_data->animation_controller->transforms, joint_count);
    shader->set_mat4("model", Mat44::identity().scale(0.004f).rotate_y(180));
    renderer->render_buffer(buffer);
    renderer->swap_buffers();
  }
}

void push_render_items(u32 map_buffer, u32 ticks, u32 map_texture)
{

  Renderer* renderer = &game_state.renderer;
  renderer->push_render_item_static(map_buffer, Mat44::identity(), map_texture);

  // load the static geometry that exists outside of the plane/map
  for (u32 i = 0; i < map.static_geometry.count; i++)
  {

    EntityRenderData* render_data = &map.static_geometry.render_data[i];
    Vector3           cube_pos    = map.static_geometry.position[i];

    Mat44             m           = Mat44::identity();
    m                             = m.scale(render_data->scale).translate(cube_pos);
    renderer->push_render_item_static(render_data->buffer_id, m, render_data->texture);
  }

  for (u32 i = 0; i < game_state.entity_count; i++)
  {
    Entity* entity = &game_state.entities[i];
    if (entity->visible)
    {
      EntityRenderData* render_data = entity->render_data;
      Mat44             m           = render_data->get_model_matrix();

      m                             = m.rotate_z(RADIANS_TO_DEGREES(entity->angle) + 90);
      m                             = m.translate(Vector3(entity->position.x, entity->position.y, 0.0f));
      if (render_data->animation_controller)
      {
        renderer->push_render_item_animated(render_data->buffer_id, m, render_data->animation_controller->transforms, render_data->animation_controller->animation_data->skeleton.joint_count,
                                            render_data->texture);
      }
      else
      {
        renderer->push_render_item_static(render_data->buffer_id, m, render_data->texture);
      }
    }
  }
  EffectNode* node = effects.head;
  EffectNode* prev = 0;
  while (node)
  {

    Effect            effect      = node->effect;
    EntityRenderData* render_data = &effect.render_data;
    Mat44             m           = render_data->get_model_matrix();
    m                             = m.rotate_z(RADIANS_TO_DEGREES(effect.angle) + 90);
    m                             = m.translate(Vector3(effect.position.x, effect.position.y, 0.0f));
    renderer->push_render_item_static(render_data->buffer_id, m, render_data->texture);

    node = node->next;
  }
}

u32 load_cubemap()
{
  u32 texture_id;
  glGenTextures(1, &texture_id);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id);

  const char* image_names[6] = {
      "./data/textures/red.tga", "./data/textures/green.tga", "./data/textures/black.tga", "./data/textures/white.tga", "./data/textures/blue.tga",
  };

  for (unsigned int i = 0; i < 6; i++)
  {
    TargaImage image = {};
    if (!sta_targa_read_from_file_rgba(&image, image_names[i % 5]))
    {
      exit(1);
    }
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data);
    GLenum error = glGetError();
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  return texture_id;
}

void test_cubemap(InputState input_state)
{

  float skyboxVertices[] = {// positions
                            -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
                            -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f, 1.0f,
                            1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f,
                            1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f, -1.0f,
                            1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

  // skybox VAO
  unsigned int skyboxVAO, skyboxVBO;
  glGenVertexArrays(1, &skyboxVAO);
  glGenBuffers(1, &skyboxVBO);
  glBindVertexArray(skyboxVAO);
  glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

  u32         cubemapTexture = load_cubemap();
  ShaderType  types[2]       = {SHADER_TYPE_VERT, SHADER_TYPE_FRAG};
  const char* locations[2]   = {
      "./shaders/skybox.vert",
      "./shaders/skybox.frag",
  };
  Shader shader(types, locations, 2, "skybox");

  shader.use();
  GLuint location = sta_glGetUniformLocation(shader.id, "skybox");
  sta_glUniform1i(location, 0);
  glActiveTexture(GL_TEXTURE0);
  sta_glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);

  f32 x = 0;
  while (true)
  {
    input_state.update();
    if (input_state.should_quit())
    {
      exit(1);
    }
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDepthFunc(GL_LEQUAL);
    shader.use();
    x -= 0.1f;
    Mat44 view = Mat44::identity().scale(0.2f).rotate_x(x).rotate_y(x).rotate_z(x);

    shader.set_mat4("view", view);
    shader.set_mat4("projection", Mat44::identity());

    glBindVertexArray(skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS); // set depth function back to default

    SDL_GL_SwapWindow(game_state.renderer.window);
  }

  exit(1);
}

int main()
{

  game_state.camera                     = Camera(Vector3(0, 0, 0), -30, -3);
  game_state.animation_controller_count = 0;

  effects.pool.init(sta_allocate(sizeof(EffectNode) * 25), sizeof(EffectNode), 25);
  command_queue.pool.init(sta_allocate(sizeof(CommandNode) * 300), sizeof(CommandNode), 300);
  command_queue.cmds         = 0;

  game_state.entity_capacity = 2;
  game_state.entities        = (Entity*)sta_allocate_struct(Entity, game_state.entity_capacity);

  const int screen_width = 620, screen_height = 480;
  game_state.renderer = Renderer(screen_width, screen_height, &logger, true);

  InputState input_state(game_state.renderer.window);

  if (!load_data())
  {
    return 1;
  }

  game_state.renderer.init_depth_texture();

  game_state.projection.perspective(45.0f, screen_width / (f32)screen_height, 0.01f, 100.0f);

  init_imgui(game_state.renderer.window, game_state.renderer.context);
  // test_cubemap(input_state);

  u32     map_shader  = game_state.renderer.get_shader_by_name("model2");
  u32     map_buffer  = get_buffer_by_name("model_map_with_pillar");
  u32     map_texture = game_state.renderer.get_texture("dirt_texture");

  Vector3 point_light_position;
  init_player(&game_state.player);

  Wave wave = {};
  if (!load_wave_from_file(&wave, "./data/waves/wave01.txt"))
  {
    logger.error("Failed to parse wave!");
    return 1;
  }

  u32      ticks        = 0;
  u32      render_ticks = 0, update_ticks = 0, build_ui_ticks = 0, ms = 0, game_running_ticks = 0, clear_tick = 0, render_ui_tick = 0;
  f32      fps      = 0.0f;

  UI_State ui_state = UI_STATE_GAME_RUNNING;
  bool     console = false, render_circle_on_mouse = false;
  char     console_buf[1024];
  memset(console_buf, 0, ArrayCount(console_buf));

  // ToDo make this work when game over is implemented
  game_state.score  = 0;

  bool debug_render = false;
  logger.info("Inited player %d", game_state.entities[game_state.player.entity].render_data->texture);

  Mat44 point_light_m         = Mat44::identity();
  point_light_position        = Vector3(-0.5f, 0.5f, 0.5);
  u32     point_light_texture = game_state.renderer.get_texture("white_texture");
  Shader* point_light_shader  = game_state.renderer.get_shader_by_index(game_state.renderer.get_shader_by_name("light_source"));
  u32     sphere_buffer       = get_buffer_by_name("model_fireball");

  // animation_test_bed(input_state);

  // light direction
  //
  u32 depth_cubemap;
  glGenTextures(1, &depth_cubemap);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, depth_cubemap);
  for (u32 i = 0; i < 6; ++i)
  {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  u32 depthMapFBO;
  glGenFramebuffers(1, &depthMapFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_cubemap, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  depth_cubemap = game_state.renderer.add_texture(depth_cubemap);

  // Figure out where you want to place both lights, one directional and one point light
  Vector3 directional_light(0.5, 0.5, 0.5);

  while (true)
  {

    if (ticks + 1 < SDL_GetTicks())
    {

      u32 tick_difference = SDL_GetTicks() - ticks;
      input_state.update();
      if (input_state.should_quit())
      {
        break;
      }

      game_state.camera.z += input_state.mouse_wheel_direction * 0.1f;
      game_state.camera.z = MAX(MIN(-1.5, game_state.camera.z), -4.5);
      if (input_state.is_key_pressed('q'))
      {
        game_state.renderer.reload_shaders();
      }

      if (game_state.entities[game_state.player.entity].hp == 0)
      {
        // ToDo Game over
        logger.info("Game over player died");
        return 1;
      }
      if (input_state.is_key_released('b'))
      {

        debug_render = !debug_render;
      }
      if (input_state.is_key_pressed('h'))
      {
        point_light_position.y -= 0.01f;
      }
      if (input_state.is_key_pressed('y'))
      {
        point_light_position.y += 0.01f;
      }

      u32 start_tick = SDL_GetTicks();
      if (ui_state == UI_STATE_GAME_RUNNING && input_state.is_key_pressed('p'))
      {
        ui_state = UI_STATE_OPTIONS_MENU;
        continue;
      }

      game_state.renderer.clear_framebuffer();
      clear_tick             = SDL_GetTicks() - start_tick;
      ui_state               = render_ui(ui_state, &game_state.player, ms, fps, update_ticks, render_ticks, game_running_ticks, screen_height, clear_tick, render_ui_tick);
      render_ui_tick         = SDL_GetTicks() - start_tick - clear_tick;

      u32 prior_render_ticks = 0;
      if (ui_state == UI_STATE_GAME_RUNNING)
      {

        assert(SDL_GetTicks() - ticks > 0 && "How is this possible?");
        game_running_ticks += SDL_GetTicks() - ticks;

        update(&wave, game_state.camera, &input_state, game_running_ticks, tick_difference);

        if (handle_wave_over(&wave))
        {
          // ToDo this should get you back to main menu or game over screen or smth
          // logger.info("Wave is over!");
          // return 0;
        }

        update_ticks       = SDL_GetTicks() - start_tick;
        prior_render_ticks = SDL_GetTicks();

        if (!debug_render)
        {
          Vector3 view_position = Vector3(-game_state.camera.translation.x, -game_state.camera.translation.y, -game_state.camera.z);
          push_render_items(map_buffer, game_running_ticks, map_texture);
          game_state.renderer.render_to_depth_texture_directional(directional_light);
          game_state.renderer.render_to_depth_texture_cube(point_light_position, depthMapFBO);

          point_light_shader->use();
          point_light_m = Mat44::identity().scale(0.1f).translate(point_light_position);
          point_light_shader->set_mat4("model", point_light_m);
          point_light_shader->set_mat4("view", game_state.camera.get_view_matrix());
          point_light_shader->set_mat4("projection", game_state.projection);
          game_state.renderer.bind_texture(*point_light_shader, "texture1", point_light_texture);
          game_state.renderer.render_buffer(sphere_buffer);

          game_state.renderer.render_queues(game_state.camera.get_view_matrix(), view_position, game_state.projection, point_light_position, depth_cubemap, directional_light);
        }

        if (render_circle_on_mouse)
        {
          i32 sdl_x, sdl_y;
          SDL_GetMouseState(&sdl_x, &sdl_y);

          f32    x = input_state.mouse_position[0];
          f32    y = input_state.mouse_position[1];
          Entity e = game_state.entities[game_state.player.entity];
          Mat44  m = Mat44::identity();
          game_state.renderer.draw_circle(Vector2(x, y), 0.05f, 2, BLUE, m, m);
        }
        if (debug_render)
        {
          debug_render_depth_texture(depth_cubemap);
          // debug_render_depth_texture_cube(depth_cubemap);
        }
      }

      if (console)
      {
        render_console(&game_state.player, &input_state, console_buf, ArrayCount(console_buf), &wave, game_running_ticks);
      }

      render_ui_frame();
      render_ticks = SDL_GetTicks() - prior_render_ticks;

      // since last frame
      ms    = SDL_GetTicks() - ticks;
      fps   = 1 / (f32)MAX(ms, 0.0001);
      ticks = SDL_GetTicks();
      game_state.renderer.swap_buffers();
    }
  }
}
