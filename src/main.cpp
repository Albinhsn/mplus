#include <GL/gl.h>
#include <GL/glext.h>
#include <sys/time.h>
#include <x86intrin.h>
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
#include "vector.h"
#include "files.h"
#include "shader.h"
#include "animation.h"
#include "collision.h"
#include "input.h"
#include "font.h"
#include "renderer.h"
#include "ui.h"

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
#include "ui.cpp"
#include "gltf.cpp"

struct Camera
{
  Vector3 translation;
  f32     rotation;
};

struct Entity;
struct Hero;

struct Ability
{
  const char* name;
  void (*use_ability)(Camera* camera, InputState* input, Hero* player);
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
  Shader         shader;
  f32            scale;
  u32            buffer_id;
  i32            animation_tick_start;
};

enum EntityType
{
  ENTITY_PLAYER,
  ENTITY_ENEMY,
  ENTITY_PLAYER_PROJECTILE,
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

u32               score;
EntityRenderData* render_data;
u32               render_data_count;
Entity*           entities;
u32               entity_capacity = 2;
u32               entity_count    = 0;
u32*              fireballs;
u32               fireball_count    = 0;
u32               fireball_capacity = 2;
bool              god               = false;

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

void find_path(Path* path, Map* map, Vector2 needle, Vector2 source)
{
  // get the tile position of both source and needle
  u8 source_x, source_y;
  u8 needle_x, needle_y;
  map->get_tile_position(source_x, source_y, source);
  map->get_tile_position(needle_x, needle_y, needle);

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
        if (!map->tiles[y][x])
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

Shader     enemy_shader;
u32        enemy_buffer_id;

TargaImage noise;

bool       is_valid_tile(Map* map, Vector2 position)
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
          return map->tiles[y][x];
        }
      }
    }
  }
  assert(!"Shouldn't be able to get here?");
}

Vector2 get_random_position(Map* map, u32 tick, u32 enemy_counter)
{
  u32     SEED_X   = 1234;
  u32     SEED_Y   = 2345;

  Vector2 position = {};
  do
  {
    u32 offset_x = ((tick + enemy_counter) * SEED_X) % (noise.width * noise.height);
    u32 offset_y = ((tick + enemy_counter) * SEED_Y) % (noise.width * noise.height);

    u8  x        = noise.data[4 * offset_x];
    u8  y        = noise.data[4 * offset_y];

    tick *= 3;

    position = Vector2((x / 255.0f) * 2.0f - 1.0f, (y / 255.0f) * 2.0f - 1.0f);

  } while (!is_valid_tile(map, position));

  return position;
}

enum EnemyType
{
  ENEMY_MELEE,
  ENEMY_FLYING,
  ENEMY_RANGED,
};

struct Enemy
{
  u32       entity;
  Path      path;
  EnemyType type;
};

void spawn(Enemy* enemy, Map* map, u32 tick, u32 enemy_index)
{
  Entity*           entity      = &entities[enemy->entity];
  EntityRenderData* render_data = entity->render_data;
  entity->velocity              = Vector2(0, 0);
  // randomize position, make sure it is valid
  entity->position           = get_random_position(map, tick, enemy_index);
  entity->render_data->scale = 0.05;
  entity->hp                 = 1;
  render_data->shader        = enemy_shader;
  render_data->buffer_id     = enemy_buffer_id;
  entity->r                  = 0.05f;
}

struct Wave
{
  u32*   spawn_times;
  u64    spawn_count;
  u32    enemy_count;
  u64    enemies_alive;
  Enemy* enemies;
};

Shader fireball_shader;
u32    fireball_id;
Map    map;

void   handle_player_movement(Renderer* renderer, Camera& camera, Hero* player, Shader* char_shader, InputState* input, u32 tick)
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

  f32* mouse_pos = input->mouse_position;
  entity->angle  = atan2f(mouse_pos[1] - entity->position.y - camera.translation.y, mouse_pos[0] - entity->position.x - camera.translation.x);

  char_shader->use();
  EntityRenderData* render_data    = entity->render_data;
  AnimationData*    animation_data = render_data->model;
  if (entity->velocity.x != 0 || entity->velocity.y != 0)
  {
    if (render_data->animation_tick_start == -1)
    {
      render_data->animation_tick_start = tick;
    }
    tick -= render_data->animation_tick_start;
    update_animation(&animation_data->skeleton, &animation_data->animations[0], *char_shader, tick);
  }
  else
  {
    Mat44 joint_transforms[animation_data->skeleton.joint_count];
    for (u32 i = 0; i < animation_data->skeleton.joint_count; i++)
    {
      joint_transforms[i].identity();
    }
    char_shader->set_mat4("jointTransforms", joint_transforms, animation_data->skeleton.joint_count);
    render_data->animation_tick_start = -1;
  }

  camera.translation                            = Vector3(-entity->position.x, -entity->position.y, 0.0);

  entities[player->entity].render_data->texture = renderer->get_texture(player->can_take_damage_tick >= SDL_GetTicks() ? "./data/blue.tga" : "./data/black.tga");
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
    // calculate distance to triangle
    // if less update closest_point
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
  if (input->is_key_pressed('q') && !on_cooldown(player->abilities[0], tick))
  {
    Ability* ability = &player->abilities[0];
    ability->use_ability(&camera, input, player);
    ability->cooldown = tick + ability->cooldown_ticks;
    logger.info("Using %s", ability->name);
  }
}

void use_ability_fireball(Camera* camera, InputState* input, Hero* player)
{
  // ToDo this can reuse dead fireballs?
  RESIZE_ARRAY(fireballs, u32, fireball_count, fireball_capacity);

  u32 entity_index            = get_new_entity();
  fireballs[fireball_count++] = entity_index;
  Entity* e                   = &entities[entity_index];
  e->type                     = ENTITY_PLAYER_PROJECTILE;

  f32    ms                   = 0.02;

  Entity player_entity        = entities[player->entity];
  e->position                 = entities[player->entity].position;
  e->velocity                 = Vector2(cosf(player_entity.angle) * ms, sinf(player_entity.angle) * ms);
  e->angle                    = player_entity.angle;
  e->r                        = 0.03f;
  e->render_data              = get_render_data_by_name("fireball");
  e->hp                       = 1;
}

void read_abilities(Ability* abilities)
{
  abilities[0].name           = "Fireball";
  abilities[0].cooldown_ticks = 1000;
  abilities[0].cooldown       = 0;
  abilities[0].use_ability    = use_ability_fireball;
}
f32 tile_position_to_game(u8 x)
{
  return (tile_size * (f32)x) * 2.0f - 1.0f;
}

void handle_enemy_movement(Map* map, Enemy* enemy, Vector2 target_position)
{
  const static f32 ms     = 0.005f;
  Entity*          entity = &entities[enemy->entity];
  enemy->path.path_count  = 0;
  find_path(&enemy->path, map, target_position, entity->position);
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
struct Sphere
{
  Vector2 position;
  f32     r;
};
bool sphere_sphere_collision(Sphere s0, Sphere s1)
{
  f32 x_diff                           = ABS(s0.position.x - s1.position.x);
  f32 y_diff                           = ABS(s0.position.y - s1.position.y);
  f32 distance_between_centers_squared = x_diff * x_diff + y_diff * y_diff;
  f32 radii_diff                       = (s0.r + s1.r) * (s0.r + s1.r);
  return distance_between_centers_squared <= radii_diff;
}

#define SHOULD_SPAWN(times, count, tick) (times[count] <= tick)

void spawn_enemies(Wave* wave, Map* map, u32 tick)
{
  u32 spawn_count = __builtin_popcount(wave->spawn_count);
  while (wave->enemy_count > spawn_count && SHOULD_SPAWN(wave->spawn_times, spawn_count, tick))
  {
    wave->enemies_alive++;
    wave->spawn_count |= spawn_count == 0 ? 1 : (1 << spawn_count);
    spawn(&wave->enemies[spawn_count], map, wave->spawn_times[spawn_count], spawn_count);
    Entity* e = &entities[wave->enemies[spawn_count].entity];
    logger.info("Spawning enemy %d at (%f, %f)\n", spawn_count, e->position.x, e->position.y);
    spawn_count++;
  }
}

void update_enemies(Map* map, Wave* wave, Hero* player, u32 tick_difference, u32 tick)
{

  Sphere player_sphere;

  player_sphere.r        = entities[player->entity].r;
  player_sphere.position = entities[player->entity].position;

  // check if we spawn
  spawn_enemies(wave, map, tick);

  Vector2 player_position = entities[player->entity].position;
  u32     spawn_count     = __builtin_popcount(wave->spawn_count);
  for (u32 i = 0; i < spawn_count; i++)
  {
    Enemy*  enemy  = &wave->enemies[i];
    Entity* entity = &entities[enemy->entity];
    if (entity->hp > 0)
    {
      // if alive update path
      // check attack
      handle_enemy_movement(map, enemy, player_position);
      Sphere enemy_sphere;
      enemy_sphere.r        = entity->r;
      enemy_sphere.position = entity->position;

      if (sphere_sphere_collision(player_sphere, enemy_sphere))
      {
        u32 tick = SDL_GetTicks();
        if (tick >= player->can_take_damage_tick && !god)
        {
          player->can_take_damage_tick = tick + player->damage_taken_cd;
          entities[player->entity].hp -= 1;
          logger.info("Took damage, hp: %d %d", entities[player->entity].hp, enemy->entity);
        }
      }
    }
  }
}

void handle_collision(Entity* e1, Entity* e2, Wave* wave)
{
  if (e1->type == ENTITY_ENEMY && e2->type == ENTITY_PLAYER_PROJECTILE)
  {
    score += 100;
    e1->hp = 0;
    e2->hp = 0;
    wave->enemies_alive -= 1;
  }
  if (e1->type == ENTITY_ENEMY && e2->type == ENTITY_ENEMY)
  {
    // get difference in position to get the vector from both centers
    //  then normalize that vector and multiply it by the radii, then subtract half of it from both
    Vector2 diff(e1->position.x - e2->position.x, e1->position.y - e2->position.y);
    diff.normalize();
    diff.scale((e1->r + e2->r + 0.001f) * 0.5f);
    // e1->position.x += diff.x;
    // e1->position.y += diff.y;
    // e2->position.x -= diff.x;
    // e2->position.y -= diff.y;
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
        if (entity->type == ENTITY_PLAYER_PROJECTILE)
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

void render_entities(Renderer* renderer, Camera camera, Mat44 camera_m)
{
  for (u32 i = 0; i < entity_count; i++)
  {
    Entity entity = entities[i];
    if (entity.hp > 0)
    {
      EntityRenderData* render_data = entity.render_data;
      render_data->shader.use();
      Mat44 m = {};
      m.identity();

      m = m.scale(render_data->scale).rotate_x(90.0f).rotate_z(RADIANS_TO_DEGREES(entity.angle) - 90).translate(Vector3(entity.position.x, entity.position.y, 0.0f));
      m = m.mul(camera_m);
      renderer->bind_texture(render_data->shader, "texture1", render_data->texture);
      render_data->shader.set_mat4("view", m);
      renderer->render_buffer(render_data->buffer_id);

      renderer->draw_circle(Vector2(entity.position.x + camera.translation.x, entity.position.y + camera.translation.y), entity.r, 1, RED);
    }
  }
}

void debug_render_map_grid(Renderer* renderer, Map* map, Camera camera)
{
  f32 tile_size = 1 / (f32)tile_count;
  for (u32 x = 0; x < tile_count; x++)
  {
    f32 x0 = x * tile_size * 2.0f - 1.0f, x1 = x0 + tile_size * 2.0f;
    x0 += camera.translation.x;
    x1 += camera.translation.x;
    for (u32 y = 0; y < tile_count; y++)
    {
      if (map->tiles[y][x])
      {
        f32 y0 = y * tile_size * 2.0f - 1.0f, y1 = y0 + tile_size * 2.0f;
        y0 += camera.translation.y;
        y1 += camera.translation.y;
        renderer->draw_line(x0, y0, x0, y1, 1, BLUE);
        renderer->draw_line(x1, y0, x1, y1, 1, BLUE);
        renderer->draw_line(x0, y1, x1, y1, 1, BLUE);
        renderer->draw_line(x0, y0, x1, y0, 1, BLUE);
      }
    }
  }
}

bool parse_wave_from_file(Wave* wave, const char* filename)
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
  assert(wave->enemy_count + 1 == lines.count && "Mismatch in wave file, expected x enemies in wave and got y");
  wave->spawn_count   = 0;
  wave->enemies_alive = 0;
  wave->spawn_times   = sta_allocate_struct(u32, wave->enemy_count);
  wave->enemies       = sta_allocate_struct(Enemy, wave->enemy_count);
  for (u32 i = 0; i < wave->enemy_count; i++)
  {
    wave->spawn_times[i]         = parse_int_from_string(lines.strings[i + 1]);
    Enemy* enemy                 = &wave->enemies[i];
    u32    entity                = get_new_entity();
    entities[entity].render_data = get_render_data_by_name("enemy");
    entities[entity].type        = ENTITY_ENEMY;

    enemy->entity                = entity;
  }
  logger.info("Read wave from '%s', got %d enemies", filename, wave->enemy_count);

  return true;
}

bool load_entity_render_data_from_file(Renderer* renderer, const char* file_location)
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
    data->name             = lines.strings[string_index++];
    if (lines.strings[string_index][0] == '0')
    {
      data->model = 0;
    }
    else
    {
      Model* model = renderer->get_model_by_filename(lines.strings[string_index]);
      assert(model->animation_data && "No animation data for the model!");
      data->model = model->animation_data;
    }
    string_index++;

    data->texture              = renderer->get_texture(lines.strings[string_index++]);
    data->shader               = *renderer->get_shader_by_name(lines.strings[string_index++]);
    data->scale                = parse_float_from_string(lines.strings[string_index++]);
    data->buffer_id            = renderer->get_buffer_by_filename(lines.strings[string_index++]);
    data->animation_tick_start = 0;
  }

  return true;
}

void render_static_geometry(Renderer* renderer, Camera camera, EntityRenderData* render_data, u32 render_data_count)
{
  for (u32 i = 0; i < render_data_count; i++)
  {
    render_data->shader.use();
    Mat44 m = {};
    m.identity();

    m = m.scale(render_data->scale).rotate_x(60);
    m = m.translate(camera.translation).translate(Vector3(0.115f, 0.115f, 0.0f));
    renderer->bind_texture(render_data->shader, "texture1", render_data->texture);
    render_data->shader.set_mat4("view", m);
    renderer->render_buffer(render_data->buffer_id);
  }
}

int main()
{

  entities  = (Entity*)sta_allocate_struct(Entity, entity_capacity);
  fireballs = (u32*)sta_allocate_struct(u32, fireball_capacity);

  AFont font;
  font.parse_ttf("./data/fonts/OpenSans-Regular.ttf");
  const int          screen_width  = 620;
  const int          screen_height = 480;
  Renderer           renderer(screen_width, screen_height, &font, &logger, true);

  const static char* model_locations = "./data/models.txt";
  if (!renderer.load_models_from_files(model_locations))
  {
    logger.error("Failed to read models from '%s'", model_locations);
    return 1;
  }

  const static char* texture_locations = "./data/textures.txt";
  if (!renderer.load_textures_from_files(texture_locations))
  {
    logger.error("Failed to read textures from '%s'", texture_locations);
    return 1;
  }

  const static char* shader_locations = "./data/shader.txt";
  if (!renderer.load_shaders_from_files(shader_locations))
  {
    logger.error("Failed to read shaders from '%s'", shader_locations);
    return 1;
  }

  const static char* buffer_locations = "./data/buffers.txt";
  if (!renderer.load_buffers_from_files(buffer_locations))
  {
    logger.error("Failed to read buffers from '%s'", buffer_locations);
    return 1;
  }

  const static char* noise_locations = "./data/noise01.tga";
  if (!sta_targa_read_from_file_rgba(&noise, noise_locations))
  {
    logger.error("Failed to read noise from '%s'", noise_locations);
    return 1;
  }

  const static char* render_data_location = "./data/render_data.txt";
  if (!load_entity_render_data_from_file(&renderer, render_data_location))
  {
    logger.error("Failed to read render data from '%s'", render_data_location);
    return 1;
  }

  InputState input_state(renderer.window);

  // ToDo make this work when game over is implemented
  score = 0;

  init_imgui(renderer.window, renderer.context);

  fireball_id       = renderer.get_buffer_by_filename("./data/fireball.obj");
  fireball_shader   = *renderer.get_shader_by_name("model");

  enemy_shader      = fireball_shader;
  enemy_buffer_id   = renderer.get_buffer_by_filename("./data/enemy.obj");

  Shader map_shader = *renderer.get_shader_by_name("model");
  Mat44  ident      = {};
  ident.identity();
  map_shader.use();
  map_shader.set_mat4("view", ident);

  map                   = {};
  u32     map_buffer    = renderer.get_buffer_by_filename("./data/map_with_hole.obj");

  u32     texture       = renderer.get_texture("./data/blizzard.tga");

  Model*  model         = renderer.get_model_by_filename("./data/model.glb");
  u32     entity_buffer = renderer.get_buffer_by_filename("./data/model.glb");

  Shader  char_shader   = *renderer.get_shader_by_name("animation");
  Vector2 char_pos(0.5, 0.5);

  Hero    player              = {};

  u32     entity_index        = get_new_entity();
  Entity* entity              = &entities[entity_index];
  player.entity               = entity_index;
  entity->type                = ENTITY_PLAYER;
  entity->render_data         = get_render_data_by_name("player");
  entity->position            = char_pos;
  entity->velocity            = Vector2(0, 0);
  player.can_take_damage_tick = 0;
  player.damage_taken_cd      = 500;
  entity->r                   = 0.05f;
  entity->hp                  = 3;
  read_abilities(player.abilities);

  Camera camera   = {};
  camera.rotation = -30.0f;

  Wave wave       = {};
  parse_wave_from_file(&wave, "./data/wave01.txt");

  map.init_map(renderer.get_model_by_filename("./data/map_with_hole.obj"));

  u32      ticks        = 0;
  u32      render_ticks = 0, update_ticks = 0, build_ui_ticks = 0, ms = 0, game_running_ticks = 0, last_tick = 0;
  f32      fps      = 0.0f;

  UI_State ui_state = UI_STATE_MAIN_MENU;
  bool     console  = false;
  char     console_buf[1024];
  memset(console_buf, 0, ArrayCount(console_buf));

  EntityRenderData pillar = *get_render_data_by_name("pillar");

  while (true)
  {
    // while (ticks + 33 > SDL_GetTicks())
    // {
    // }

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

      u32 start_tick = SDL_GetTicks();
      if (ui_state == UI_STATE_GAME_RUNNING && input_state.is_key_pressed('p'))
      {
        ui_state = UI_STATE_OPTIONS_MENU;
        continue;
      }

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplSDL2_NewFrame();
      ImGui::NewFrame();

      u32 prior_render_ticks = 0;
      if (ui_state == UI_STATE_GAME_RUNNING)
      {
        game_running_ticks += SDL_GetTicks() - ticks;
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
        ImGui::Text("HP: %d ", entities[player.entity].hp);
        ImGui::Text("Score:%d", (i32)score);
        ImGui::PopStyleColor();

        ImGui::GetFont()->Scale = old_size;
        ImGui::PopFont();
        ImGui::End();

        u64 width       = 40;
        u64 height      = 40;
        u64 total_width = ArrayCount(player.abilities) * width;
        ImGui::SetNextWindowPos(ImVec2(center.x - (u64)(total_width / 2), screen_height - height), ImGuiCond_Appearing);
        // ImGui::SetNextWindowSize(ImVec2(center.x, center.y));
        ImGui::Begin("Abilities", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
        for (u32 i = 0; i < ArrayCount(player.abilities); i++)
        {
          char buf[64];
          memset(buf, 0, ArrayCount(buf));
          snprintf(buf, ArrayCount(buf), "%.1f", MAX(0, ((i32)player.abilities[i].cooldown - (i32)game_running_ticks) / 1000.0f));

          ImGui::Button(buf, ImVec2(width, height));
          ImGui::SameLine();
        }
        ImGui::End();

        handle_abilities(camera, &input_state, &player, game_running_ticks);
        handle_player_movement(&renderer, camera, &player, &char_shader, &input_state, ticks);
        update_entities(entities, entity_count, &wave, tick_difference);

        update_enemies(&map, &wave, &player, tick_difference, game_running_ticks);

        u32 spawn_count = __builtin_popcount(wave.spawn_count);
        if (spawn_count == wave.enemy_count && wave.enemies_alive == 0)
        {
          // ToDo this should get you back to main menu or game over screen or smth
          logger.info("Wave is over!");
          return 0;
        }
        Mat44 camera_m = {};
        camera_m.identity();
        camera_m           = camera_m.rotate_x(camera.rotation).translate(camera.translation);

        update_ticks       = SDL_GetTicks() - start_tick;
        prior_render_ticks = SDL_GetTicks();

        map_shader.use();
        map_shader.set_mat4("view", camera_m);

        // render map
        renderer.bind_texture(map_shader, "texture1", texture);
        renderer.render_buffer(map_buffer);
        render_entities(&renderer, camera, camera_m);

        render_static_geometry(&renderer, camera, &pillar, 1);
      }
      else if (ui_state == UI_STATE_MAIN_MENU)
      {

        // ToDo render hp and score

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(ImVec2(center.x, center.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGui::Begin("Main menu!", 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);

        ImGui::Text("MPLUS!");
        if (ImGui::Button("Run game!"))
        {
          ui_state = UI_STATE_GAME_RUNNING;
        }

        ImGui::End();
      }
      else if (ui_state == UI_STATE_OPTIONS_MENU)
      {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(ImVec2(center.x, center.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGui::Begin("Main menu!", 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);

        if (ImGui::Button("Return!"))
        {
          ui_state = UI_STATE_GAME_RUNNING;
        }

        ImGui::End();
      }

      if (console)
      {
        if (input_state.is_key_released(ASCII_RETURN))
        {
          logger.info("Released buffer: %s", console_buf);
          if (compare_strings("restart", console_buf))
          {
            logger.info("Restarting wave");
            wave.enemies_alive = 0;
            u32 spawn_count    = __builtin_popcount(wave.spawn_count);
            for (u32 i = 0; i < spawn_count; i++)
            {
              Entity* e = &entities[wave.enemies[i].entity];
              e->hp     = 0;
            }
            wave.spawn_count           = 0;
            game_running_ticks         = 0;
            entities[player.entity].hp = 3;
            score                      = 0;
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
            printf("!\n");
            logger.info("Debug on");
          }
          memset(console_buf, 0, ArrayCount(console_buf));
        }
        ImGui::Begin("Console!", 0, ImGuiWindowFlags_NoDecoration);
        if (ImGui::InputText("Console input", console_buf, ArrayCount(console_buf)))
        {
        }

        ImGui::End();
      }

      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      render_ticks = SDL_GetTicks() - prior_render_ticks;

      // since last frame
      ms        = SDL_GetTicks() - last_tick;
      fps       = 1 / (f32)MAX(ms, 0.0001);
      ticks     = SDL_GetTicks();
      last_tick = SDL_GetTicks();
      renderer.swap_buffers();
    }
  }
}
