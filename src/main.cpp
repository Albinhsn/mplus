#include <GL/gl.h>
#include <GL/glext.h>
#include <stdio.h>
#include <sys/time.h>
#include <x86intrin.h>
#include <sys/mman.h>
#include <assert.h>
#include <cfloat>
#include <stdlib.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <SDL2/SDL.h>

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#define IMGUI_DEFINE_MATH_OPERATORS

#include "platform.h"
#include "common.cpp"
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

u32 score;

struct Camera
{
  Vector3 translation;
};

struct Entity;
struct Entities;

struct Ability
{
  const char* name;
  void (*use_ability)(Camera* camera, Entities* entities, InputState* input, Entity* entity);
  u32 cooldown_ticks;
  u32 cooldown;
};

struct Entity
{
  AnimationModel* animated_model;
  Shader          shader;
  Ability         abilities[5];
  // position + r is sphere bb
  Vector2 position;
  f32     r;
  Vector2 velocity;
  u32     flags;
  f32     scale;
  i32     animation_tick_start;
  u32     buffer_id;
  u32     can_take_damage_tick;
  u32     damage_taken_cd;
  u32     id;
  u32     parent_id;
  u32     hp;
};

const static u32 tile_count = 64;
const static f32 tile_size  = 1.0f / tile_count;
struct Map
{
public:
  Map()
  {
    model = 0;
  }
  void init_map()
  {
    assert(model && "Can't init map without model");
    u32         vertex_count = model->vertex_count;
    VertexData* vertices     = model->vertices;
    u32*        indices      = model->indices;
    for (u32 x = 0; x < tile_count; x++)
    {
      f32 x0 = x * tile_size * 2.0f - 1.0f, x1 = x0 + tile_size * 2.0f;
      for (u32 y = 0; y < tile_count; y++)
      {
        f32 y0 = y * tile_size * 2.0f - 1.0f, y1 = y0 + tile_size * 2.0f;
        f32 x_middle = x0 + (x1 - x0) / 2.0f;
        f32 y_middle = y0 + (y1 - y0) / 2.0f;

        for (u32 i = 0; i < vertex_count; i += 3)
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
  ModelData* model;
  bool       tiles[tile_count][tile_count];
  void       get_tile_position(u8& tile_x, u8& tile_y, Vector2 position)
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
    // printf("Looking for path!\n");
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
struct Enemy
{
  Entity entity;
  Path   path;
};

Shader enemy_shader;
u32    enemy_buffer_id;

struct EnemyNode
{
  Enemy      enemy;
  EnemyNode* next;
};

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

struct Enemies
{
public:
  Enemies()
  {
    head          = 0;
    enemy_counter = 0;
  }
  void spawn(Map* map, u32 tick)
  {
    EnemyNode* en    = (EnemyNode*)sta_pool_alloc(&pool);
    Entity*    enemy = &en->enemy.entity;
    enemy->velocity  = Vector2(0, 0);
    // randomize position, make sure it is valid
    enemy->position  = get_random_position(map, tick, ++enemy_counter);
    enemy->scale     = 0.05;
    enemy->shader    = enemy_shader;
    enemy->buffer_id = enemy_buffer_id;
    enemy->hp        = 1;
    enemy->r         = 0.04f;

    en->next         = head;
    head             = en;
  }
  u32           enemy_counter;
  EnemyNode*    head;
  PoolAllocator pool;
};

struct EntityNode
{
  Entity      entity;
  EntityNode* next;
};

struct Entities
{
public:
  Entities()
  {
    head           = 0;
    entity_counter = 0;
  }
  u32 add(EntityNode* entity)
  {
    entity->next = head;
    head         = entity;
    return entity_counter++;
  }
  PoolAllocator pool;
  u32           entity_counter;
  EntityNode*   head;
};

struct Wave
{
  u32* spawn_times;
  u32  enemy_count;
  u64  spawn_count;
};

ModelData fireball;
Shader    fireball_shader;
u32       fireball_id;
Map       map;

void      handle_movement(Camera& camera, Entity* entity, Shader* char_shader, InputState* input, u32 tick)
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

  f32* mouse_pos = input->mouse_position;
  f32  angle     = atan2f(mouse_pos[1] - entity->position.y - camera.translation.y, mouse_pos[0] - entity->position.x - camera.translation.x);
  // the velocity should be based on the direction as well

  entity->position.x += entity->velocity.x;
  entity->position.y += entity->velocity.y;

  char_shader->use();
  if (entity->velocity.x != 0 || entity->velocity.y != 0)
  {
    if (entity->animation_tick_start == -1)
    {
      entity->animation_tick_start = tick;
    }
    tick -= entity->animation_tick_start;
    update_animation(*entity->animated_model, *char_shader, tick);
  }
  else
  {
    Mat44 joint_transforms[entity->animated_model->skeleton.joint_count];
    for (u32 i = 0; i < entity->animated_model->skeleton.joint_count; i++)
    {
      joint_transforms[i].identity();
    }
    char_shader->set_mat4("jointTransforms", joint_transforms, entity->animated_model->skeleton.joint_count);
    entity->animation_tick_start = -1;
  }

  Mat44 a = {};
  a.identity();
  a                  = a.scale(0.03f).rotate_x(90.0f).rotate_z(RADIANS_TO_DEGREES(angle) - 90);
  camera.translation = Vector3(-entity->position.x, -entity->position.y, 0.0);
  char_shader->set_mat4("view", a);
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

bool out_of_map(Vector2* closest_point, Map* map, Vector2 position)
{
  ModelData*  map_data = map->model;
  VertexData* vertices = map_data->vertices;
  u32*        indices  = map_data->indices;
  f32         distance = FLT_MAX;
  for (u32 i = 0; i < map_data->vertex_count - 2; i += 3)
  {
    Triangle t;
    t.points[0].x = vertices[indices[i]].vertex.x;
    t.points[0].y = vertices[indices[i]].vertex.y;
    t.points[1].x = vertices[indices[i + 1]].vertex.x;
    t.points[1].y = vertices[indices[i + 1]].vertex.y;
    t.points[2].x = vertices[indices[i + 2]].vertex.x;
    t.points[2].y = vertices[indices[i + 2]].vertex.y;
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

void detect_collision(Map* map, Vector2& position)
{
  Vector2 closest_point = {};
  if (out_of_map(&closest_point, map, position))
  {
    position = closest_point;
  }
}

static inline bool on_cooldown(Ability ability, u32 tick)
{
  return tick < ability.cooldown;
}

void handle_abilities(Camera camera, Entities* entities, InputState* input, Entity* entity, u32 tick)
{
  if (input->is_key_pressed('q') && !on_cooldown(entity->abilities[0], tick))
  {
    entity->abilities[0].use_ability(&camera, entities, input, entity);
    entity->abilities[0].cooldown = tick + entity->abilities[0].cooldown_ticks;
    printf("Using %s\n", entity->abilities[0].name);
  }
}

void use_ability_fireball(Camera* camera, Entities* entities, InputState* input, Entity* entity)
{
  EntityNode* en        = (EntityNode*)sta_pool_alloc(&entities->pool);
  Entity*     e         = &en->entity;
  f32         ms        = 0.03;

  f32*        mouse_pos = input->mouse_position;
  f32         angle     = atan2f(mouse_pos[1] - entity->position.y - camera->translation.y, mouse_pos[0] - entity->position.x - camera->translation.x);
  e->id                 = entities->add(en);
  e->position           = entity->position;
  e->velocity           = Vector2(cosf(angle) * ms, sinf(angle) * ms);
  e->buffer_id          = fireball_id;
  e->shader             = fireball_shader;
  e->scale              = 0.05f;
  e->r                  = 0.03f;
}

void read_abilities(Ability* abilities)
{
  abilities[0].name           = "Fireball";
  abilities[0].cooldown_ticks = 1000;
  abilities[0].cooldown       = 0;
  abilities[0].use_ability    = use_ability_fireball;
}
f32 tile_position_to_game_centered(u8 x)
{
  return (tile_size * (f32)x) * 2.0f - 1.0f;
}

void handle_enemy_movement(Map* map, Enemy* enemy, Vector2 target_position)
{
  const static f32 ms = 0.005f;
  find_path(&enemy->path, map, target_position, enemy->entity.position);
  Path path = enemy->path;
  if (path.path_count == 1)
  {
    return;
  }

  u32     path_idx           = 2;
  Vector2 curr               = enemy->entity.position;
  f32     movement_remaining = ms;
  while (true)
  {
    // printf("Enemy movement!\n");
    f32 x = tile_position_to_game_centered(path.path[path_idx][0]);
    f32 y = tile_position_to_game_centered(path.path[path_idx][1]);
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
      curr.x                 = curr.x + velocity.x;
      curr.y                 = curr.y + velocity.y;
      enemy->entity.position = curr;
      return;
    }
    // convert current position to curr
    movement_remaining -= moved;
    curr = point;
    if (path_idx >= path.path_count)
    {
      return;
    }
  }
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

void update_enemies(Map* map, Enemies* enemies, Entity* player)
{
  EnemyNode* node = enemies->head;
  EnemyNode* prev = 0;

  Sphere     player_sphere;
  player_sphere.r        = player->r;
  player_sphere.position = player->position;

  while (node)
  {
    if (node->enemy.entity.hp == 0)
    {
      if (prev)
      {
        prev->next = node->next;
      }
      else
      {
        enemies->head = node->next;
      }
      EnemyNode* new_node = node->next;
      sta_pool_free(&enemies->pool, (u64)node);
      node = new_node;
      continue;
    }

    handle_enemy_movement(map, &node->enemy, player->position);

    Sphere enemy_sphere;
    enemy_sphere.r        = node->enemy.entity.r;
    enemy_sphere.position = node->enemy.entity.position;

    if (sphere_sphere_collision(player_sphere, enemy_sphere))
    {
      u32 tick = SDL_GetTicks();
      if (tick >= player->can_take_damage_tick)
      {
        player->can_take_damage_tick = tick + player->damage_taken_cd;
        player->hp -= 1;
        printf("Took damage! %d\n", player->hp);
      }
    }

    prev = node;
    node = node->next;
  }
}

void render_enemies(Renderer* renderer, Enemies* enemies, Camera camera)
{
  EnemyNode* node = enemies->head;
  while (node)
  {

    Entity* entity = &node->enemy.entity;
    entity->shader.use();
    Color green = BLUE;
    entity->shader.set_float4f("color", (float*)&green);
    Mat44 pos = {};
    pos.identity();
    entity->shader.set_mat4("projection", pos);
    pos = pos.scale(entity->scale).rotate_x(90).translate(camera.translation).translate(Vector3(entity->position.x, entity->position.y, 0));
    entity->shader.set_mat4("view", pos);
    renderer->render_buffer(node->enemy.entity.buffer_id);

    Color   red = RED;
    Vector2 position(entity->position.x + camera.translation.x, entity->position.y + camera.translation.y);
    renderer->draw_circle(position, entity->r, 1, red);

#if DEBUG
    Path path = node->enemy.path;
    for (u32 i = 0; i < path.path_count - 1; i++)
    {

      f32 x0 = tile_position_to_game_centered(path.path[i][0]) + camera.translation.x;
      f32 y0 = tile_position_to_game_centered(path.path[i][1]) + camera.translation.y;
      f32 x1 = tile_position_to_game_centered(path.path[i + 1][0]) + camera.translation.x;
      f32 y1 = tile_position_to_game_centered(path.path[i + 1][1]) + camera.translation.y;
      renderer->draw_line(x0, y0, x1, y1, 4, RED);
    }

#endif

    node = node->next;
  }
}

bool handle_entity_collision(Entity* entity, Enemies* enemies)
{
  EnemyNode* enemy = enemies->head;
  Sphere     entity_bb;
  entity_bb.position = entity->position;
  entity_bb.r        = entity->r;
  while (enemy)
  {

    Sphere enemy_bb;
    enemy_bb.position = enemy->enemy.entity.position;
    enemy_bb.r        = enemy->enemy.entity.r;

    if (sphere_sphere_collision(entity_bb, enemy_bb))
    {
      score += 100;
      enemy->enemy.entity.hp = 0;
      return true;
    }
    enemy = enemy->next;
  }
  return false;
}

void update_entities(Entities* entities, Enemies* enemies)
{
  EntityNode* node = entities->head;
  EntityNode* prev = 0;
  while (node)
  {
    Entity* entity = &node->entity;
    entity->position.x += entity->velocity.x;
    entity->position.y += entity->velocity.y;

    Vector2 closest_point = {};
    if (handle_entity_collision(entity, enemies) || out_of_map(&closest_point, &map, entity->position))
    {
      if (prev)
      {
        prev->next = node->next;
      }
      else
      {
        entities->head = 0;
      }

      sta_pool_free(&entities->pool, (u64)entity);
    }

    prev = node;
    node = node->next;
  }
}

void render_entities(Renderer* renderer, Entities* entities, Camera camera)
{
  EntityNode* node = entities->head;
  while (node)
  {
    Entity entity = node->entity;
    entity.shader.use();
    Mat44 m = {};
    m.identity();
    entity.shader.set_mat4("projection", m);

    m           = m.scale(entity.scale);
    m           = m.translate(camera.translation).translate(Vector3(entity.position.x, entity.position.y, 0.0f));
    Color green = GREEN;
    entity.shader.set_float4f("color", (float*)&green);
    entity.shader.set_mat4("view", m);
    renderer->render_buffer(entity.buffer_id);

    renderer->draw_circle(Vector2(entity.position.x + camera.translation.x, entity.position.y + camera.translation.y), entity.r, 1, RED);

    node = node->next;
  }
}

#define SHOULD_SPAWN(times, count, tick) (times[count] <= tick)

void spawn_enemies(Wave* wave, Map* map, Enemies* enemies, u32 tick)
{
  u32 spawn_count = __builtin_popcount(wave->spawn_count);
  while (wave->enemy_count > spawn_count && SHOULD_SPAWN(wave->spawn_times, spawn_count, tick))
  {
    wave->spawn_count |= (1 << ++spawn_count);
    enemies->spawn(map, wave->spawn_times[spawn_count - 1]);
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

  // ToDo make this work when game over is implemented
  score = 0;

  init_imgui(renderer.window, renderer.context);

  if (!sta_targa_read_from_file_rgba(&noise, "./data/noise01.tga"))
  {
    printf("Failed to read noise!\n");
    return 1;
  }

  BufferAttributes map_attributes[3] = {
      {3, GL_FLOAT},
      {2, GL_FLOAT},
      {3, GL_FLOAT}
  };
  map       = {};
  map.model = (ModelData*)sta_allocate_struct(ModelData, 1);
  sta_parse_wavefront_object_from_file(map.model, "./data/map_with_hole.obj");
  fireball = {};
  sta_parse_wavefront_object_from_file(&fireball, "./data/fireball.obj");
  fireball_id     = renderer.create_buffer_indices(sizeof(VertexData) * fireball.vertex_count, fireball.vertices, fireball.vertex_count, fireball.indices, map_attributes, ArrayCount(map_attributes));
  fireball_shader = Shader("./shaders/model2.vert", "./shaders/model2.frag");

  enemy_shader    = fireball_shader;
  ModelData enemy_model = {};
  if (!sta_parse_wavefront_object_from_file(&enemy_model, "./data/enemy.obj"))
  {
    printf("Failed to parse enemy!\n");
    return 1;
  }
  enemy_buffer_id =
      renderer.create_buffer_indices(sizeof(VertexData) * enemy_model.vertex_count, enemy_model.vertices, enemy_model.vertex_count, enemy_model.indices, map_attributes, ArrayCount(map_attributes));

  Shader map_shader("./shaders/model.vert", "./shaders/model.frag");
  Mat44  ident = {};
  ident.identity();
  map_shader.use();
  map_shader.set_mat4("view", ident);
  map_shader.set_mat4("projection", ident);
  TargaImage image = {};

  sta_targa_read_from_file_rgba(&image, "./data/blizzard.tga");
  u32 map_buffer =
      renderer.create_buffer_indices(sizeof(VertexData) * map.model->vertex_count, map.model->vertices, map.model->vertex_count, map.model->indices, map_attributes, ArrayCount(map_attributes));
  u32            texture = renderer.create_texture(image.width, image.height, image.data);
  AnimationModel model   = {};
  gltf_parse(&model, "./data/model.glb");
  BufferAttributes animated_attributes[5] = {
      {3, GL_FLOAT},
      {2, GL_FLOAT},
      {3, GL_FLOAT},
      {4, GL_FLOAT},
      {4,   GL_INT}
  };
  u32 entity_buffer =
      renderer.create_buffer_indices(sizeof(SkinnedVertex) * model.vertex_count, model.vertices, model.index_count, model.indices, animated_attributes, ArrayCount(animated_attributes));
  Shader  char_shader("./shaders/animation.vert", "./shaders/animation.frag");
  Vector2 char_pos(0.5, 0.5);
  Entity  entity              = {};
  entity.animated_model       = &model;
  entity.position             = char_pos;
  entity.animation_tick_start = 0;
  entity.id                   = 0;
  entity.velocity             = Vector2(0, 0);
  entity.can_take_damage_tick = 0;
  entity.damage_taken_cd      = 500;
  entity.r                    = 0.04f;
  entity.hp                   = 3;
  read_abilities(entity.abilities);

  Camera   camera   = {};
  Entities entities = {};
  entities.head     = 0;
  sta_pool_init(&entities.pool, sta_allocate(sizeof(EntityNode) * 100), sizeof(EntityNode), 100);
  Enemies enemies = {};
  enemies.head    = 0;
  sta_pool_init(&enemies.pool, sta_allocate(sizeof(EnemyNode) * 100), sizeof(EnemyNode), 100);

  Wave wave                 = {};
  u32  wave_spawn_times[10] = {100, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000};
  wave.spawn_times          = &wave_spawn_times[0];
  wave.enemy_count          = ArrayCount(wave_spawn_times);
  wave.spawn_count          = 0;
  map.init_map();

  u32      render_ticks = 0, update_ticks = 0, build_ui_ticks = 0, ms = 0;
  u32      game_running_ticks = 0;
  f32      fps                = 0.0f;

  UI_State ui_state           = UI_STATE_MAIN_MENU;

  bool     console            = false;

  char     console_buf[1024];
  memset(console_buf, 0, ArrayCount(console_buf));
  while (true)
  {

    if (ticks + 16 < SDL_GetTicks())
    {
      input_state.update();
      if (input_state.should_quit())
      {
        break;
      }
      renderer.clear_framebuffer();
      if (entity.hp == 0)
      {
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

        // ToDo render hp and score
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(ImVec2(center.x, center.y * 0.5f), 0, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Game ui!", 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
        ImGui::Text("HP: %d ", entity.hp);
        ImGui::Text("Score: %d", (i32)score);
        ImGui::PopStyleColor();

        ImGui::End();

        handle_abilities(camera, &entities, &input_state, &entity, ticks);
        handle_movement(camera, &entity, &char_shader, &input_state, ticks);
        update_entities(&entities, &enemies);
        spawn_enemies(&wave, &map, &enemies, game_running_ticks);

        update_enemies(&map, &enemies, &entity);
        detect_collision(&map, entity.position);
        u32 spawn_count = __builtin_popcount(wave.spawn_count);
        if (spawn_count == wave.enemy_count && enemies.head == 0)
        {
          // ToDo this should get you back to main menu or game over screen or smth
          printf("YOU WON\n");
          return 0;
        }

        update_ticks       = SDL_GetTicks() - start_tick;
        prior_render_ticks = SDL_GetTicks();

        Mat44 m            = {};
        m.identity();
        m = m.translate(camera.translation);
        map_shader.use();
        map_shader.set_mat4("view", m);

        // render map
        renderer.bind_texture(texture, 0);
        renderer.render_buffer(map_buffer);
        render_entities(&renderer, &entities, camera);
        render_enemies(&renderer, &enemies, camera);

        // render entity

        char_shader.use();
        Color color = entity.can_take_damage_tick >= SDL_GetTicks() ? BLUE : BLACK;
        char_shader.set_float4f("color", (float*)&color);
        renderer.render_buffer(entity_buffer);

        renderer.draw_circle(Vector2(entity.position.x + camera.translation.x, entity.position.y + camera.translation.y), entity.r * 2, 1, RED);
      }
      else if (ui_state == UI_STATE_MAIN_MENU)
      {
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
          printf("%s\n", console_buf);
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
      renderer.swap_buffers();
      render_ticks = SDL_GetTicks() - prior_render_ticks;

      ms           = SDL_GetTicks() - start_tick;
      fps          = 1 / (f32)MAX(ms, 0.0001);
      ticks        = SDL_GetTicks() + 16;
    }
  }
}
