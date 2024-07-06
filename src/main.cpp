#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL_mouse.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <strings.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

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
    z           = _z;
  }

public:
  Mat44 get_view_matrix()
  {
    Mat44 camera_m = {};
    camera_m.identity();
    return camera_m.rotate_x(rotation).translate(Vector3(translation.x, translation.y, z));
  }
  Vector3 translation;
  f32     rotation;
  f32     z;
};

struct Entity;
struct Hero;

struct Ability
{
  const char* name;
  bool (*use_ability)(Camera* camera, InputState* input, Hero* player, u32 ticks);
  u32 cooldown_ticks;
  u32 cooldown;
};

struct Hero
{
  Ability abilities[5];
  u32     can_take_damage_tick;
  u32     damage_taken_cd;
  u32     entity;
};

struct EntityRenderData
{
  char*          name;
  AnimationData* model;
  u32            texture;
  u32            shader;
  f32            scale;
  u32            buffer_id;
  i32            animation_tick_start;
};

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
  u32               hp;
  EntityRenderData* render_data;
  EntityType        type;
};

Camera            camera(Vector3(), -20.0f, -3.0f);
Hero              player = {};
Mat44             projection;
Renderer          renderer;
u32               score;

Model*            models;
u32               model_count;
RenderBuffer*     buffers;
u32               buffer_count;
EntityRenderData* render_data;
u32               render_data_count;

Entity*           entities;
u32               entity_capacity = 2;
u32               entity_count    = 0;
bool              god             = false;

#include "ui.cpp"
#include "ui.h"

u32 get_buffer_by_name(const char* filename)
{
  for (u32 i = 0; i < buffer_count; i++)
  {
    if (compare_strings(buffers[i].model_name, filename))
    {
      return buffers[i].buffer_id;
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

  return MODEL_FILE_UNKNOWN;
}
Model* get_model_by_name(const char* filename)
{
  for (u32 i = 0; i < model_count; i++)
  {
    if (compare_strings(filename, models[i].name))
    {
      return &models[i];
    }
  }
  logger.error("Didn't find model '%s'", filename);
  return 0;
}

bool load_models_from_files(const char* file_location)
{

  Buffer      buffer = {};
  StringArray lines  = {};
  if (!sta_read_file(&buffer, file_location))
  {
    return false;
  }
  split_buffer_by_newline(&lines, &buffer);

  model_count = parse_int_from_string(lines.strings[0]);

  models      = sta_allocate_struct(Model, model_count);
  logger.info("Found %d models", model_count);
  for (u32 i = 0, string_index = 1; i < model_count; i++)
  {
    // ToDo free the loaded memory?
    char*               model_name     = lines.strings[string_index++];
    char*               model_location = lines.strings[string_index++];
    ModelFileExtensions extension      = get_model_file_extension(model_location);
    Model*              model          = &models[i];
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
      for (u32 i = 0; i < model_data.vertex_count; i++)
      {
        model->vertices[i] = model_data.vertices[i].vertex;
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
      for (u32 i = 0; i < model_data.vertex_count; i++)
      {
        model->vertices[i] = model_data.vertices[i].position;
      }
      model->animation_data           = (AnimationData*)sta_allocate_struct(AnimationData, 1);
      model->animation_data->skeleton = model_data.skeleton;
      // ToDo this should change once you fixed the parser
      model->animation_data->animation_count = 1;
      model->animation_data->animations      = (Animation*)sta_allocate_struct(Animation, model->animation_data->animation_count);
      model->animation_data->animations[0]   = model_data.animations;
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
  Buffer      buffer = {};
  StringArray lines  = {};
  if (!sta_read_file(&buffer, file_location))
  {
    return false;
  }
  split_buffer_by_newline(&lines, &buffer);

  assert(lines.count > 0 && "Buffers file had no content?");

  u32 count    = parse_int_from_string(lines.strings[0]);

  buffers      = (RenderBuffer*)sta_allocate_struct(RenderBuffer, count);
  buffer_count = count;

  for (u32 i = 0, string_index = 1; i < count; i++)
  {
    char*            model_name             = lines.strings[string_index++];
    Model*           model                  = get_model_by_name(model_name);
    u32              buffer_attribute_count = parse_int_from_string(lines.strings[string_index++]);
    BufferAttributes attributes[buffer_attribute_count];
    for (u32 j = 0; j < buffer_attribute_count; j++)
    {
      char* line = lines.strings[string_index];
      string_index++;
      Buffer buffer(line, strlen(line));
      attributes[j].count = buffer.parse_int();
      buffer.skip_whitespace();

      // ToDo check validity
      attributes[j].type = (BufferAttributeType)buffer.parse_int();
    }
    buffers[i].model_name = model_name;
    buffers[i].buffer_id  = renderer.create_buffer_from_model(model, attributes, buffer_attribute_count);
    logger.info("Loaded buffer '%s', id: %d", model_name, buffers[i].buffer_id);
  }
  return true;
}

EntityRenderData* get_render_data_by_name(const char* name)
{
  for (u32 i = 0; i < render_data_count; i++)
  {
    if (compare_strings(render_data[i].name, name))
    {
      return &render_data[i];
    }
  }
  logger.error("Couldn't find render data with name '%s'", name);
  assert(!"Couldn't find render data!");
}

u32 get_new_entity()
{
  RESIZE_ARRAY(entities, Entity, entity_count, entity_capacity);
  return entity_count++;
}

const static u32 tile_count = 64;
const static f32 tile_size  = 1.0f / tile_count;

struct Map
{
public:
  Map()
  {
  }
  u32*     indices;
  Vector2* vertices;
  u32      index_count;
  u32      vertex_count;
  void     init_map(Model* model)
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

    for (u32 x = 0; x < tile_count; x++)
    {
      f32 x0 = x * tile_size * 2.0f - 1.0f, x1 = x0 + tile_size * 2.0f;
      for (u32 y = 0; y < tile_count; y++)
      {
        f32 y0 = y * tile_size * 2.0f - 1.0f, y1 = y0 + tile_size * 2.0f;
        f32 x_middle = x0 + (x1 - x0) / 2.0f;
        f32 y_middle = y0 + (y1 - y0) / 2.0f;

        for (u32 i = 0; i < index_count; i += 3)
        {
          Triangle t;
          t.points[0].x = vertices[indices[i]].vertex.x;
          t.points[0].y = vertices[indices[i]].vertex.y;
          t.points[1].x = vertices[indices[i + 1]].vertex.x;
          t.points[1].y = vertices[indices[i + 1]].vertex.y;
          t.points[2].x = vertices[indices[i + 2]].vertex.x;
          t.points[2].y = vertices[indices[i + 2]].vertex.y;
          tiles[y][x]   = point_in_triangle_2d(t, Vector2(x_middle, y_middle));
          if (tiles[y][x])
          {
            break;
          }
        }
      }
    }
  }
  bool tiles[tile_count][tile_count];
  void get_tile_position(u8& tile_x, u8& tile_y, Vector2 position)
  {
    for (u32 y = 0; y < tile_count; y++)
    {
      f32 min_y = -1.0f + tile_size * y * 2.0f;
      if (min_y <= position.y && position.y <= min_y + tile_size * 2.0f)
      {
        for (u32 x = 0; x < tile_count; x++)
        {
          f32 min_x = -1.0f + tile_size * x * 2.0f;
          if (min_x <= position.x && position.x <= min_x + tile_size * 2.0f)
          {
            tile_x = x;
            tile_y = y;
            return;
          }
        }
        break;
      }
    }
    assert(!"Was out of bounds!\n");
  }
};
Map map;

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
  u8  path[tile_count * 4][2];
  u32 path_count;
};

void find_path(Path* path, Vector2 needle, Vector2 source)
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
        path->path[i][0] = curr.path[i] >> 8;
        path->path[i][1] = curr.path[i] & 0xFF;
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
        if (!map.tiles[y][x])
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

bool       is_valid_tile(Vector2 position)
{
  f32 tile_size = 2.0f / tile_count;
  for (u32 y = 0; y < tile_count; y++)
  {
    if (-1.0f + tile_size * y <= position.y && position.y <= -1.0f + tile_size * (y + 1))
    {
      for (u32 x = 0; x < tile_count; x++)
      {
        if (-1.0f + tile_size * x <= position.x && position.x <= -1.0f + tile_size * (x + 1))
        {
          return map.tiles[y][x];
        }
      }
    }
  }
  return false;
}

Vector2 get_random_position(u32 tick, u32 enemy_counter)
{
  u32     SEED_X             = 1234;
  u32     SEED_Y             = 2345;

  f32     min_valid_distance = 0.5f;

  Vector2 position           = {};

  Vector2 player_position    = entities[player.entity].position;

  f32     distance_to_player;
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
    if (min_valid_distance > distance_to_player)
    {
      logger.info("%f vs %f\n", min_valid_distance, distance_to_player);
    }

  } while (!is_valid_tile(position) && distance_to_player >= min_valid_distance);

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
  u32    enemy_count;
  u64    enemies_alive;
  Enemy* enemies;
};

void spawn(Wave* wave, u32 enemy_index)
{
  Enemy*  enemy  = &wave->enemies[enemy_index];
  Entity* entity = &entities[enemy->entity];
  wave->spawn_count |= enemy_index == 0 ? 1 : (1 << enemy_index);
  wave->enemies_alive++;
  EntityRenderData* render_data = entity->render_data;
  entity->position              = get_random_position(wave->spawn_times[enemy_index], enemy_index);
  entity->hp                    = enemy->initial_hp;
  logger.info("Spawning enemy %d at (%f, %f) %d", enemy_index, entity->position.x, entity->position.y, wave->enemies_alive);
  if (enemy->type == ENEMY_RANGED)
  {
    enemy->cooldown = 1000;
  }
}

enum CommandType
{
  CMD_SPAWN_ENEMY,
  CMD_LET_RANGED_MOVE_AFTER_SHOOTING,
  CMD_EXPLODE_COC
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
  PoolAllocator pool;
  CommandNode*  head;
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
struct CommandExplodeCoCData
{
  Vector2 position;
};

void add_command(CommandType type, void* data, u32 tick)
{
  CommandNode* node          = (CommandNode*)command_queue.pool.alloc();
  node->command.type         = type;
  node->command.data         = data;
  node->command.execute_tick = tick;

  node->next                 = command_queue.head;
  command_queue.head         = node;
}

void run_command_explode_coc(void* data)
{
  CommandExplodeCoCData* coc_data = (CommandExplodeCoCData*)data;
  Sphere                 coc_sphere;
  coc_sphere.position = coc_data->position;
  coc_sphere.r        = 0.2f;
  // iterate over the entities
  for (u32 i = 0; i < entity_count; i++)
  {
    if (entities[i].type == ENTITY_ENEMY && entities[i].hp > 0)
    {
      Sphere enemy_sphere;
      enemy_sphere.position = entities[i].position;
      enemy_sphere.r        = entities[i].r;
      if (sphere_sphere_collision(coc_sphere, enemy_sphere))
      {
        score += 100;
        entities[i].hp = 0;
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
      case CMD_EXPLODE_COC:
      {
        run_command_explode_coc(node->command.data);
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
      }

      if (prev)
      {
        prev->next = node->next;
      }
      else
      {
        command_queue.head = node->next;
      }
      CommandNode* next = node->next;
      command_queue.pool.free((u64)node);
      node = next;
      continue;
    }
    prev = node;
    node = node->next;
  }
}

void handle_player_movement(Camera& camera, Hero* player, InputState* input, u32 tick)
{
  Entity* entity   = &entities[player->entity];
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

  f32* mouse_pos                   = input->mouse_position;
  entity->angle                    = atan2f(mouse_pos[1] - entity->position.y - camera.translation.y, mouse_pos[0] - entity->position.x - camera.translation.x);

  EntityRenderData* render_data    = entity->render_data;
  Shader*           shader         = renderer.get_shader_by_index(render_data->shader);
  AnimationData*    animation_data = render_data->model;
  if (animation_data)
  {
    if (entity->velocity.x != 0 || entity->velocity.y != 0)
    {
      if (render_data->animation_tick_start == -1)
      {
        render_data->animation_tick_start = tick;
      }
      tick -= render_data->animation_tick_start;
      update_animation(&animation_data->skeleton, &animation_data->animations[0], *shader, tick);
    }
    else
    {
      Mat44 joint_transforms[animation_data->skeleton.joint_count];
      for (u32 i = 0; i < animation_data->skeleton.joint_count; i++)
      {
        joint_transforms[i].identity();
      }
      shader->set_mat4("jointTransforms", joint_transforms, animation_data->skeleton.joint_count);
      render_data->animation_tick_start = -1;
    }
  }

  camera.translation                            = Vector3(-entity->position.x, -entity->position.y, 0.0);

  entities[player->entity].render_data->texture = renderer.get_texture(player->can_take_damage_tick >= SDL_GetTicks() ? "blue_texture" : "black_texture");
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

bool is_out_of_map_bounds(Vector2* closest_point, Map* map, Vector2 position)
{
  f32 distance = FLT_MAX;
  for (u32 i = 0; i < map->vertex_count - 2; i += 3)
  {
    Triangle t;
    t.points[0].x = map->vertices[map->indices[i]].x;
    t.points[0].y = map->vertices[map->indices[i]].y;
    t.points[1].x = map->vertices[map->indices[i + 1]].x;
    t.points[1].y = map->vertices[map->indices[i + 1]].y;
    t.points[2].x = map->vertices[map->indices[i + 2]].x;
    t.points[2].y = map->vertices[map->indices[i + 2]].y;
    if (point_in_triangle_2d(t, position))
    {
      return false;
    }
    Vector2 v   = closest_point_triangle(t, position);
    f32     len = v.sub(position).len();
    if (len < distance)
    {
      *closest_point = v;
      distance       = len;
    }
  }
  return true;
}

void detect_collision_out_of_bounds(Map* map, Vector2& position)
{
  Vector2 closest_point = {};
  if (is_out_of_map_bounds(&closest_point, map, position))
  {
    position = closest_point;
  }
}

static inline bool on_cooldown(Ability ability, u32 tick)
{
  return tick < ability.cooldown;
}

void handle_abilities(Camera camera, InputState* input, Hero* player, u32 tick)
{
  const char keybinds[] = {'1', '2', '3'};
  for (u32 i = 0; i < ArrayCount(keybinds); i++)
  {
    if (input->is_key_pressed(keybinds[i]) && !on_cooldown(player->abilities[i], tick))
    {
      Ability* ability = &player->abilities[i];
      if (ability->use_ability(&camera, input, player, tick))
      {
        ability->cooldown = tick + ability->cooldown_ticks;
        logger.info("Using %s", ability->name);
      }
    }
  }
}
bool use_ability_blink(Camera* camera, InputState* input, Hero* player, u32 ticks)
{

  Entity    player_entity = entities[player->entity];
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
    if (is_out_of_map_bounds(&closest_point, &map, position))
    {
      position = closest_point;
      break;
    }
  }
  entities[player->entity].position = position;
  return true;
}

bool use_ability_fireball(Camera* camera, InputState* input, Hero* player, u32 ticks)
{
  // ToDo this can reuse dead fireballs?

  u32     entity_index = get_new_entity();
  Entity* e            = &entities[entity_index];
  e->type              = ENTITY_PLAYER_PROJECTILE;

  f32    ms            = 0.02;

  Entity player_entity = entities[player->entity];
  e->position          = entities[player->entity].position;
  e->velocity          = Vector2(cosf(player_entity.angle) * ms, sinf(player_entity.angle) * ms);
  e->angle             = player_entity.angle;
  e->r                 = 0.03f;
  e->render_data       = get_render_data_by_name("fireball");
  e->hp                = 1;
  return true;
}

struct Effect
{
  EntityRenderData render_data;
  Vector2          position;
  f32              angle;
  u32              effect_ends_at;
  void (*update_effect)(Effect* effect, u32 tick);
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

bool use_ability_cone_of_cold(Camera* camera, InputState* input, Hero* player, u32 ticks)
{

  Effect effect;
  effect.update_effect = 0;
  effect.render_data   = *get_render_data_by_name("cone of cold");
  Entity  entity       = entities[player->entity];

  Mat44   view         = camera->get_view_matrix().mul(projection);

  Vector2 point        = {};
  if (!find_cursor_position_on_map(point, input->mouse_position, view))
  {
    return false;
  }

  CommandExplodeCoCData* data = sta_allocate_struct(CommandExplodeCoCData, 1);
  data->position              = Vector2(point.x, point.y);
  effect.position             = Vector2(point.x, point.y);

  effect.angle                = entity.angle;
  effect.effect_ends_at       = ticks + 500;

  EffectNode* node            = (EffectNode*)effects.pool.alloc();
  node->next                  = effects.head;
  effects.head                = node;

  node->effect                = effect;

  // position
  add_command(CMD_EXPLODE_COC, (void*)data, effect.effect_ends_at);
  return true;
}

void read_abilities(Ability* abilities)
{
  abilities[0].name           = "Fireball";
  abilities[0].cooldown_ticks = 1000;
  abilities[0].cooldown       = 0;
  abilities[0].use_ability    = use_ability_fireball;

  abilities[1].name           = "Blink";
  abilities[1].cooldown_ticks = 5000;
  abilities[1].cooldown       = 0;
  abilities[1].use_ability    = use_ability_blink;

  abilities[2].name           = "Cone of Cold";
  abilities[2].cooldown_ticks = 2000;
  abilities[2].cooldown       = 0;
  abilities[2].use_ability    = use_ability_cone_of_cold;
}
f32 tile_position_to_game(u8 x)
{
  return (tile_size * (f32)x) * 2.0f - 1.0f;
}

void handle_enemy_movement(Enemy* enemy, Vector2 target_position)
{
  const static f32 ms     = 0.005f;
  Entity*          entity = &entities[enemy->entity];
  enemy->path.path_count  = 0;
  find_path(&enemy->path, target_position, entity->position);
  Path path = enemy->path;
  if (path.path_count == 1)
  {
    return;
  }

  u32     path_idx           = 2;
  Vector2 prev_position      = entity->position;
  Vector2 curr               = entity->position;
  f32     movement_remaining = ms;
  while (true)
  {
    f32 x = tile_position_to_game(path.path[path_idx][0]);
    f32 y = tile_position_to_game(path.path[path_idx][1]);
    path_idx++;
    if (compare_float(x, curr.x) && compare_float(y, curr.y))
    {
      continue;
    }
    Vector2 point(x, y);

    // distance from curr to point
    f32 moved = (ABS(point.x - curr.x) + ABS(point.y - curr.y)) / tile_size;
    if (moved > movement_remaining)
    {
      Vector2 velocity(point.x - curr.x, point.y - curr.y);
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

bool player_is_visible(f32& angle, Vector2 position, Hero* player)
{

  const f32 step_size     = 0.005f;
  Entity    player_entity = entities[player->entity];
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
    if (is_out_of_map_bounds(&closest_point, &map, position))
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

void update_enemies(Wave* wave, Hero* player, u32 tick_difference, u32 tick)
{

  Sphere player_sphere;

  player_sphere.r        = entities[player->entity].r;
  player_sphere.position = entities[player->entity].position;

  // check if we spawn

  Vector2 player_position = entities[player->entity].position;
  u32     spawn_count     = __builtin_popcount(wave->spawn_count);
  for (u32 i = 0; i < spawn_count; i++)
  {
    Enemy*  enemy  = &wave->enemies[i];
    Entity* entity = &entities[enemy->entity];
    if (entity->hp > 0)
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
        if (sphere_sphere_collision(player_sphere, enemy_sphere))
        {
          u32 tick = SDL_GetTicks();
          if (tick >= player->can_take_damage_tick && !god)
          {
            player->can_take_damage_tick = tick + player->damage_taken_cd;
            // entities[player->entity].hp -= 1;
            // logger.info("Took damage, hp: %d %d", entities[player->entity].hp, enemy->entity);
          }
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
          if (player_is_visible(angle, entity->position, player))
          {

            enemy->can_move                               = false;

            CommandLetRangedMoveAfterShooting* arrow_data = sta_allocate_struct(CommandLetRangedMoveAfterShooting, 1);
            arrow_data->enemy                             = enemy;
            add_command(CMD_LET_RANGED_MOVE_AFTER_SHOOTING, (void*)arrow_data, tick + 300);
            u32     entity_index  = get_new_entity();
            Entity* entity        = &entities[enemy->entity];
            Entity* e             = &entities[entity_index];
            e->type               = ENTITY_ENEMY_PROJECTILE;

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
    score += 100;
    e1->hp = 0;
    e2->hp = 0;
    wave->enemies_alive -= 1;
  }
  // ToDo physics sim?
  if (e1->type == ENTITY_ENEMY && e2->type == ENTITY_ENEMY)
  {
  }
  if ((e1->type == ENTITY_PLAYER && e2->type == ENTITY_ENEMY_PROJECTILE) || (e1->type == ENTITY_ENEMY_PROJECTILE && e2->type == ENTITY_PLAYER))
  {
    e2->hp = 0;
  }
}

void update_entities(Entity* entities, u32 entity_count, Wave* wave, u32 tick_difference)
{

  for (u32 i = 0; i < entity_count; i++)
  {

    Entity* entity = &entities[i];
    if (entity->hp > 0)
    {
      f32 diff = (f32)tick_difference / 16.0f;
      entity->position.x += entity->velocity.x * diff;
      entity->position.y += entity->velocity.y * diff;
      Vector2 closest_point = {};
      if (is_out_of_map_bounds(&closest_point, &map, entity->position))
      {
        if (entity->type == ENTITY_PLAYER_PROJECTILE || entity->type == ENTITY_ENEMY_PROJECTILE)
        {
          entity->hp = 0;
        }
        entity->position = closest_point;
      }
    }
  }
  for (u32 i = 0; i < entity_count - 1; i++)
  {
    Entity* e1 = &entities[i];
    if (e1->hp == 0)
    {
      continue;
    }
    Sphere e1_sphere;
    e1_sphere.r        = e1->r;
    e1_sphere.position = e1->position;
    for (u32 j = i + 1; j < entity_count; j++)
    {
      Entity* e2 = &entities[j];
      if (e2->hp == 0)
      {
        continue;
      }
      Sphere e2_sphere;
      e2_sphere.r        = e2->r;
      e2_sphere.position = e2->position;
      if (sphere_sphere_collision(e1_sphere, e2_sphere))
      {
        handle_collision(e1, e2, wave);
        if (e1->hp == 0)
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
  Buffer      buffer = {};
  StringArray lines  = {};
  if (!sta_read_file(&buffer, filename))
  {
    return false;
  }
  split_buffer_by_newline(&lines, &buffer);

  u32 count        = parse_int_from_string(lines.strings[0]);
  enemy_data_count = count;
  enemy_data       = sta_allocate_struct(EnemyData, count);

  for (u32 i = 0, string_index = 1; i < count; i++)
  {
    EnemyData* data        = &enemy_data[i];
    data->type             = (EnemyType)parse_int_from_string(lines.strings[string_index++]);
    data->hp               = parse_int_from_string(lines.strings[string_index++]);
    data->radius           = parse_float_from_string(lines.strings[string_index++]);
    data->render_data_name = lines.strings[string_index++];
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
  assert(wave->enemy_count * 2 + 1 == lines.count && "Mismatch in wave file, expected x enemies in wave and got y");
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

    wave->spawn_times[i]        = parse_int_from_string(lines.strings[string_index++]);
    u32 entity_idx              = get_new_entity();
    enemy->entity               = entity_idx;

    Entity* entity              = &entities[entity_idx];
    entity->render_data         = get_render_data_by_name(enemy_data.render_data_name);
    entity->type                = ENTITY_ENEMY;
    entity->angle               = 0.0f;
    entity->hp                  = 0;
    entity->r                   = enemy_data.radius;
    entity->velocity            = Vector2(0, 0);

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

  Buffer      buffer = {};
  StringArray lines  = {};
  if (!sta_read_file(&buffer, file_location))
  {
    return false;
  }
  split_buffer_by_newline(&lines, &buffer);

  assert(lines.count > 0 && "Empty render data file?");
  u32 count         = parse_int_from_string(lines.strings[0]);
  render_data       = sta_allocate_struct(EntityRenderData, count);
  render_data_count = count;
  for (u32 i = 0, string_index = 1; i < count; i++)
  {
    EntityRenderData* data = &render_data[i];
    // name
    // animation
    // texture
    // shader
    // scale
    // buffer
    data->name = lines.strings[string_index++];
    if (lines.strings[string_index][0] == '0')
    {
      data->model = 0;
    }
    else
    {
      Model* model = get_model_by_name(lines.strings[string_index]);
      assert(model->animation_data && "No animation data for the model!");
      data->model = model->animation_data;
    }
    string_index++;

    const char* texture        = lines.strings[string_index++];
    const char* shader         = lines.strings[string_index++];
    const char* scale          = lines.strings[string_index++];
    const char* buffer_id      = lines.strings[string_index++];
    data->texture              = renderer.get_texture(texture);
    data->shader               = renderer.get_shader_by_name(shader);
    data->scale                = parse_float_from_string((char*)scale);
    data->buffer_id            = get_buffer_by_name(buffer_id);
    data->animation_tick_start = 0;
    logger.info("Loaded render data for '%s': texture: %s, shader: %s, scale: %s,  buffer: %s", data->name, texture, shader, scale, buffer_id);
  }

  return true;
}

static f32 p = PI;

bool       load_data()
{

  const static char* enemy_data_location = "./data/formats/enemy.txt";
  if (!load_enemies_from_file(enemy_data_location))
  {
    logger.error("Failed to read models from '%s'", enemy_data_location);
    return false;
  }

  const static char* model_locations = "./data/formats/models.txt";
  if (!load_models_from_files(model_locations))
  {
    logger.error("Failed to read models from '%s'", model_locations);
    return false;
  }

  const static char* texture_locations = "./data/formats/textures.txt";
  if (!renderer.load_textures_from_files(texture_locations))
  {
    logger.error("Failed to read textures from '%s'", texture_locations);
    return false;
  }

  const static char* shader_locations = "./data/formats/shader.txt";
  if (!renderer.load_shaders_from_files(shader_locations))
  {
    logger.error("Failed to read shaders from '%s'", shader_locations);
    return false;
  }

  const static char* buffer_locations = "./data/formats/buffers.txt";
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

  const static char* render_data_location = "./data/formats/render_data.txt";
  if (!load_entity_render_data_from_file(render_data_location))
  {
    logger.error("Failed to read render data from '%s'", render_data_location);
    return false;
  }
  return true;
}

void init_player(Hero* player)
{

  u32     entity_index         = get_new_entity();
  Entity* entity               = &entities[entity_index];
  player->entity               = entity_index;
  entity->type                 = ENTITY_PLAYER;
  entity->render_data          = get_render_data_by_name("player");
  entity->position             = Vector2(0.5, 0.5);
  entity->velocity             = Vector2(0, 0);
  player->can_take_damage_tick = 0;
  player->damage_taken_cd      = 500;
  entity->r                    = 0.05f;
  entity->hp                   = 3;
  read_abilities(player->abilities);
}

Mat44   light_space_matrix;
Vector3 light_position;
void    render_to_depth_buffer(Vector3 light_position)
{

  glCullFace(GL_FRONT);
  renderer.change_viewport(renderer.shadow_width, renderer.shadow_height);
  glBindFramebuffer(GL_FRAMEBUFFER, renderer.shadow_map_framebuffer);
  glClear(GL_DEPTH_BUFFER_BIT);

  static u32 vao = 0, vbo = 0, ebo = 0;
  if (vao == 0)
  {
    sta_glGenVertexArrays(1, &vao);
    sta_glGenBuffers(1, &vbo);
    sta_glGenBuffers(1, &ebo);
    sta_glBindVertexArray(vao);
    f32 a[5] = {};
    sta_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    sta_glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * ArrayCount(a), a, GL_DYNAMIC_DRAW);
    sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * ArrayCount(a), a, GL_DYNAMIC_DRAW);

    sta_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(f32) * 3, (void*)(sizeof(f32) * 0));
    sta_glEnableVertexAttribArray(0);

    sta_glBindVertexArray(0);
  }
  Shader depth_shader = *renderer.get_shader_by_index(renderer.get_shader_by_name("depth"));
  Mat44  m            = {};
  m.identity();
  depth_shader.use();

  Mat44 l            = Mat44::look_at(light_position, Vector3(0, 0, 0), Vector3(0, 1, 0));
  Mat44 o            = Mat44::orthographic(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 7.5f);
  light_space_matrix = l.mul(o);

  // bind the light space matrix
  depth_shader.set_mat4("lightSpaceMatrix", light_space_matrix);
  depth_shader.set_mat4("model", m);

  sta_glBindVertexArray(vao);

  Model* map_model = get_model_by_name("model_map_with_pillar");
  sta_glBindBuffer(GL_ARRAY_BUFFER, vbo);
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(Vector3) * map_model->vertex_count, map_model->vertices, GL_DYNAMIC_DRAW);
  sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * map_model->index_count, map_model->indices, GL_DYNAMIC_DRAW);
  glDrawElements(GL_TRIANGLES, map_model->index_count, GL_UNSIGNED_INT, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  renderer.reset_viewport_to_screen_size();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glCullFace(GL_BACK);
}

void render_map(u32 map_shader_index, u32 map_texture, u32 map_buffer, Vector3 view_position)
{

  Shader* map_shader = renderer.get_shader_by_index(map_shader_index);

  map_shader->use();
  Mat44   m;
  Vector3 ambient_lighting(0.25, 0.25, 0.25);
  m.identity();
  map_shader->set_vec3("ambient_lighting", ambient_lighting);
  map_shader->set_vec3("light_position", light_position);
  map_shader->set_mat4("light_space_matrix", light_space_matrix);
  map_shader->set_mat4("model", m);
  map_shader->set_mat4("view", camera.get_view_matrix());
  map_shader->set_mat4("projection", projection);

  // render map
  renderer.bind_texture(*map_shader, "texture1", map_texture);
  renderer.bind_texture(*map_shader, "shadow_map", renderer.depth_texture);
  // map_shader->set_vec3("viewPos", view_position);
  renderer.render_buffer(map_buffer);
}

void render_entities()
{
  for (u32 i = 0; i < entity_count; i++)
  {
    Entity entity = entities[i];
    if (entity.hp > 0)
    {
      EntityRenderData* render_data = entity.render_data;
      Shader*           shader      = renderer.get_shader_by_index(render_data->shader);
      shader->use();
      Mat44 m = {};
      m.identity();
      m = m.scale(render_data->scale);
      m = m.rotate_z(RADIANS_TO_DEGREES(entity.angle) + 90);
      m = m.translate(Vector3(entity.position.x, entity.position.y, 0.0f));

      renderer.bind_texture(*shader, "texture1", render_data->texture);
      renderer.bind_texture(*shader, "shadow_map", renderer.depth_texture);
      shader->set_mat4("model", m);
      shader->set_mat4("view", camera.get_view_matrix());
      shader->set_mat4("projection", projection);
      shader->set_mat4("light_space_matrix", light_space_matrix);
      // shader->set_vec3("viewPos", camera.translation);
      renderer.render_buffer(render_data->buffer_id);

      renderer.draw_circle(entity.position, entity.r, 1, RED, camera.get_view_matrix(), projection);
    }
  }
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
      u32 spawn_count     = __builtin_popcount(wave->spawn_count);
      for (u32 i = 0; i < spawn_count; i++)
      {
        Entity* e = &entities[wave->enemies[i].entity];
        e->hp     = 0;
      }
      wave->spawn_count           = 0;
      game_running_ticks          = 0;
      entities[player->entity].hp = 3;
      score                       = 0;
    }
    else if (compare_strings("vsync", console_buf))
    {
      renderer.toggle_vsync();
      logger.info("toggled vsync to %d!\n", renderer.vsync);
    }
    else if (compare_strings("god", console_buf))
    {
      god = !god;
      logger.info("godmode toggled to %d!\n", god);
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
      renderer.reload_shaders();
    }
    memset(console_buf, 0, console_buf_size);
  }
  ImGui::Begin("Console!", 0, ImGuiWindowFlags_NoDecoration);
  if (ImGui::InputText("Console input", console_buf, console_buf_size))
  {
  }

  ImGui::End();
}

void update(Wave* wave, Camera& camera, InputState* input_state, Hero* player, u32& game_running_ticks, u32 ticks, u32 tick_difference)
{

  handle_abilities(camera, input_state, player, game_running_ticks);
  handle_player_movement(camera, player, input_state, ticks);
  run_commands(game_running_ticks);
  update_entities(entities, entity_count, wave, tick_difference);
  update_enemies(wave, player, tick_difference, game_running_ticks);
}

inline bool handle_wave_over(Wave* wave)
{
  u32 spawn_count = __builtin_popcount(wave->spawn_count);
  return spawn_count == wave->enemy_count && wave->enemies_alive == 0;
}

void debug_render_depth_texture()
{
  Shader q_shader = *renderer.get_shader_by_index(renderer.get_shader_by_name("quad"));
  q_shader.use();

  renderer.bind_texture(q_shader, "texture1", renderer.depth_texture);
  renderer.enable_2d_rendering();
  static u32 vao = 0, vbo = 0, ebo = 0;

  if (vao == 0)
  {
    sta_glGenVertexArrays(1, &vao);
    sta_glGenBuffers(1, &vbo);
    sta_glGenBuffers(1, &ebo);
    sta_glBindVertexArray(vao);
    f32 tot[20] = {
        1.0f,  1.0f,  0, 1.0f, 1.0f, //
        1.0f,  -1.0,  0, 1.0f, 0.0f, //
        -1.0f, -1.0f, 0, 0.0f, 0.0f, //
        -1.0f, 1.0,   0, 0.0f, 1.0f, //
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

  sta_glBindVertexArray(vao);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  renderer.disable_2d_rendering();
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
    Shader*           shader      = renderer.get_shader_by_index(render_data->shader);
    shader->use();
    Mat44 m = {};
    m.identity();
    m = m.scale(render_data->scale);
    m = m.rotate_z(RADIANS_TO_DEGREES(effect.angle) + 90);
    m = m.translate(Vector3(effect.position.x, effect.position.y, 0.0f));

    renderer.bind_texture(*shader, "texture1", render_data->texture);
    shader->set_mat4("model", m);
    shader->set_mat4("view", camera.get_view_matrix());
    shader->set_mat4("projection", projection);
    renderer.render_buffer(render_data->buffer_id);

    node = node->next;
  }
}

int main()
{

  effects.pool.init(sta_allocate(sizeof(EffectNode) * 25), sizeof(EffectNode), 25);
  command_queue.pool.init(sta_allocate(sizeof(CommandNode) * 25), sizeof(CommandNode), 25);

  entities               = (Entity*)sta_allocate_struct(Entity, entity_capacity);

  const int screen_width = 620, screen_height = 480;
  renderer = Renderer(screen_width, screen_height, &logger, true);
  if (!load_data())
  {
    return 1;
  }
  renderer.init_depth_texture();

  projection.perspective(45.0f, screen_width / (f32)screen_height, 0.01f, 100.0f);

  init_imgui(renderer.window, renderer.context);

  u32    map_shader  = renderer.get_shader_by_name("model2");
  Model* map_model   = get_model_by_name("model_map_with_pillar");
  u32    map_buffer  = get_buffer_by_name("model_map_with_pillar");
  u32    map_texture = renderer.get_texture("dirt_texture");

  init_player(&player);

  Wave wave = {};
  if (!load_wave_from_file(&wave, "./data/waves/wave01.txt"))
  {
    logger.error("Failed to parse wave!");
    return 1;
  }

  map.init_map(get_model_by_name("model_map_with_hole"));

  u32      ticks        = 0;
  u32      render_ticks = 0, update_ticks = 0, build_ui_ticks = 0, ms = 0, game_running_ticks = 0;
  f32      fps      = 0.0f;

  UI_State ui_state = UI_STATE_GAME_RUNNING;
  bool     console = false, render_circle_on_mouse = false;
  char     console_buf[1024];
  memset(console_buf, 0, ArrayCount(console_buf));
  InputState input_state(renderer.window);

  // ToDo make this work when game over is implemented
  score             = 0;

  bool debug_render = false;

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
      renderer.clear_framebuffer();
      if (entities[player.entity].hp == 0)
      {
        // ToDo Game over
        logger.info("Game over player died");
        return 1;
      }
      if (input_state.is_key_pressed('c'))
      {
        console = true;
      }
      if (input_state.is_key_released('b'))
      {
        debug_render = !debug_render;
      }
      if (input_state.is_key_pressed('l'))
      {
        camera.rotation -= 1;
        logger.info("New rotation %f", camera.rotation);
      }
      if (input_state.is_key_released('x'))
      {
        render_circle_on_mouse = !render_circle_on_mouse;
        logger.info("Rendering circle on mouse");
      }

      u32 start_tick = SDL_GetTicks();
      if (ui_state == UI_STATE_GAME_RUNNING && input_state.is_key_pressed('p'))
      {
        ui_state = UI_STATE_OPTIONS_MENU;
        continue;
      }

      ui_state               = render_ui(ui_state, &player, ms, fps, update_ticks, render_ticks, game_running_ticks, screen_height);

      u32 prior_render_ticks = 0;
      if (ui_state == UI_STATE_GAME_RUNNING)
      {

        if (p < -PI)
        {
          p = PI;
        }
        p -= 0.01f;
        light_position = Vector3(cosf(p) * 2, sinf(p) * 2, 1);

        game_running_ticks += SDL_GetTicks() - ticks;

        update(&wave, camera, &input_state, &player, game_running_ticks, ticks, tick_difference);

        if (handle_wave_over(&wave))
        {
          // ToDo this should get you back to main menu or game over screen or smth
          logger.info("Wave is over!");
          return 0;
        }

        update_ticks       = SDL_GetTicks() - start_tick;
        prior_render_ticks = SDL_GetTicks();

        render_to_depth_buffer(light_position);

        render_map(map_shader, map_texture, map_buffer, camera.translation);
        render_entities();
        render_effects(game_running_ticks);
        if (render_circle_on_mouse)
        {
          i32 sdl_x, sdl_y;
          SDL_GetMouseState(&sdl_x, &sdl_y);

          f32    x = input_state.mouse_position[0];
          f32    y = input_state.mouse_position[1];
          Entity e = entities[player.entity];
          Mat44  m = {};
          m.identity();
          renderer.draw_circle(Vector2(x, y), 0.1f, 2, BLUE, m, m);
        }
        if (debug_render)
        {
          debug_render_depth_texture();
        }
      }

      if (console)
      {
        render_console(&player, &input_state, console_buf, ArrayCount(console_buf), &wave, game_running_ticks);
      }

      render_ui_frame();
      render_ticks = SDL_GetTicks() - prior_render_ticks;

      // since last frame
      ms    = SDL_GetTicks() - ticks;
      fps   = 1 / (f32)MAX(ms, 0.0001);
      ticks = SDL_GetTicks();
      renderer.swap_buffers();
    }
  }
}
