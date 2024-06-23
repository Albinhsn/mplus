#include "collision.h"
#include <cassert>
#include <cstdlib>

inline float cross_2d(Point2 u, Point2 v)
{
  return u.y * v.x - u.x * v.y;
}

inline bool point_in_triangle_2d(Triangle t, Point2 p)
{
  // printf("Triangle:\n");
  // t.points[0].debug();
  // t.points[1].debug();
  // t.points[2].debug();
  // printf("-\n");
  // p.debug();
  if (cross_2d(p.sub(t.points[0]), t.points[1].sub(t.points[0])) > 0.0f)
  {
    // printf("Failed first\n");
    return false;
  }
  if (cross_2d(p.sub(t.points[1]), t.points[2].sub(t.points[1])) > 0.0f)
  {
    // printf("Failed second\n");
    return false;
  }
  if (cross_2d(p.sub(t.points[2]), t.points[0].sub(t.points[2])) > 0.0f)
  {
    // printf("Failed third\n");
    return false;
  }
  return true;
}

inline float orient_2d(Vector2 v0, Vector2 v1, Vector2 p)
{
  return (v0.x - p.x) * (v1.y - p.y) - (v0.y - p.y) * (v1.x - p.x);
}
int translate(PointDLL* point)
{
  return !point ? -1 : point->idx;
}

char translate(int i)
{
  char letters[] = "abcdefghijklmnopqrstuvwz12345";
  assert(i <= ArrayCount(letters));
  return letters[i];
}

void debug_points(Triangulation* tri)
{
  for (u32 i = 0; i < tri->count; i++)
  {
    PointDLL* head = &tri->head[i];
    if (head->prev)
    {
      printf("%c\t|%c|\t%c ---- ", translate(head->prev->idx), translate(head->idx), translate(head->next->idx));
      if (head->next_ear)
      {
        printf("EAR, ");
      }
      if (head->next_convex || head->prev_convex)
      {
        printf("CONVEX, ");
      }
      if (head->next_reflex || head->prev_reflex)
      {
        printf("REFLEX");
      }
      printf("-\n");
    }
  }

  printf("-----\n");
}

static inline void insert_node_cyclical(PointDLL* list, Vector2 v, u32 idx, u32 point_count)
{
  list[idx].point = v;
  list[idx].next  = &list[(idx + 1) % point_count];
  list[idx].prev  = &list[idx - 1 < 0 ? 0 : idx - 1];
  list[idx].idx   = idx;
}

void get_vertices(Triangulation* tri, Vector2* v_points, u32 point_count)
{
  tri->head      = (PointDLL*)sta_allocate_struct(PointDLL, point_count);
  tri->count     = point_count;

  tri->ear       = 0;
  tri->convex    = 0;
  tri->reflex    = 0;

  PointDLL* curr = 0;
  for (i32 i = 0; i < point_count; i++)
  {
    insert_node_cyclical(tri->head, v_points[i], i, point_count);
    curr         = &tri->head[i];
    curr->idx    = i;

    i32 i_low    = i - 1 < 0 ? point_count - 1 : i - 1;
    i32 i_high   = (i + 1) % point_count;
    curr->prev   = &tri->head[i_low];
    curr->next   = &tri->head[i_high];

    Vector2 v0   = v_points[i_low];
    Vector2 v1   = v_points[i];
    Vector2 v2   = v_points[i_high];
    f32     v0v1 = orient_2d(v0, v1, v2);
    f32     v1v2 = orient_2d(v1, v2, v0);

    // test if any other vertex is inside the triangle from these vertices

    Triangle t(v0, v1, v2);

    if (v0v1 < 0 && v1v2 < 0)
    {
      if (tri->convex != 0)
      {
        tri->convex->next_convex = curr;
        curr->prev_convex        = tri->convex;
        tri->convex              = curr;
      }
      else
      {
        tri->convex = curr;
      }
      bool found = false;
      for (i32 j = 0; j < point_count; j++)
      {
        // only need to test reflex vertices?
        if (j != i && j != i_low && j != i_high && point_in_triangle_2d(t, v_points[j]))
        {
          found = true;
          break;
        }
      }

      if (!found)
      {
        if (tri->ear != 0)
        {
          tri->ear->next_ear = curr;
          curr->prev_ear     = tri->ear;
          tri->ear           = curr;
        }
        else
        {
          tri->ear = curr;
        }
      }
    }
    else
    {
      if (tri->reflex != 0)
      {
        tri->reflex->next_reflex = curr;
        curr->prev_reflex        = tri->reflex;
        tri->reflex              = curr;
      }
      else
      {
        tri->reflex = curr;
      }
    }
  }

  bool e = false, c = false, r = false;
  for (u32 i = 0; i < point_count; i++)
  {
    if (tri->head[i].next_ear && !e)
    {
      tri->ear->next_ear    = &tri->head[i];
      tri->head[i].prev_ear = tri->ear;
      e                     = true;
    }
    if (tri->head[i].next_convex && !c)
    {
      tri->convex->next_convex = &tri->head[i];
      tri->head[i].prev_convex = tri->convex;
      e                        = true;
    }
    if (tri->head[i].next_reflex && !r)
    {
      tri->reflex->next_reflex = &tri->head[i];
      tri->head[i].prev_reflex = tri->reflex;
      r                        = true;
    }
    if (e && c && r)
    {
      break;
    }
  }
}

PointDLL* get_reflex_vertex(PointDLL* vertices, u32 point_count, PointDLL* current)
{

  for (u32 i = 0; i < point_count; i++)
  {
    if ((vertices[i].next_reflex || vertices[i].prev_reflex) && current != &vertices[i])
    {
      return &vertices[i];
    }
  }
  return 0;
}
PointDLL* get_convex_vertex(PointDLL* vertices, u32 point_count)
{
  for (u32 i = 0; i < point_count; i++)
  {
    if (vertices[i].next_convex || vertices[i].prev_convex)
    {
      return &vertices[i];
    }
  }
  assert(!"Found no convex vertex?");
}
PointDLL* get_ear_vertex(PointDLL* vertices, u32 point_count, PointDLL* current_ear)
{
  for (u32 i = 0; i < point_count; i++)
  {
    if ((vertices[i].next_ear || vertices[i].prev_ear) && &vertices[i] != current_ear)
    {
      return &vertices[i];
    }
  }
  assert(!"Found no ear vertex?");
}

bool is_ear(PointDLL* reflex, PointDLL* vertex)
{
  Triangle tri = {vertex->prev->point, vertex->point, vertex->next->point};
  printf("Testing if %c is in still ear\n", translate(vertex->idx));
  u64 start = (u64)reflex;
  if (reflex)
  {
    do
    {
      if (reflex != vertex->prev && reflex != vertex->next)
      {
        printf("Testing %c\n", translate(reflex->idx));
        if (point_in_triangle_2d(tri, reflex->point))
        {
          printf("Found point inside!\n");
          return false;
        }
      }
      else
      {
        printf("Was prev or next! %c\n", translate(reflex->idx));
      }
      reflex = reflex->next_reflex;
    } while ((u64)reflex != start && reflex);
  }
  return true;
}

bool is_convex(PointDLL* vertex)
{

  Vector2 v0   = vertex->prev->point;
  Vector2 v1   = vertex->point;
  Vector2 v2   = vertex->next->point;
  f32     v0v1 = orient_2d(v0, v1, v2);
  f32     v1v2 = orient_2d(v1, v2, v0);

  return v0v1 < 0 && v1v2 < 0;
}

void attach_ear(Triangulation* tri, PointDLL* vertex)
{
  PointDLL* head_ear = tri->ear;
  if (head_ear == 0)
  {
    vertex->next_ear = vertex;
    vertex->prev_ear = vertex;
    tri->ear         = vertex;
  }
  else
  {
    head_ear->next_ear->prev_ear = vertex;
    vertex->next_ear             = head_ear->next_ear;
    head_ear->next_ear           = vertex;
    vertex->prev_ear             = head_ear;
  }
}

void test_vertex(PointDLL* vertex, Triangulation* tri)
{

  // might not be an ear, but still always convex?
  if (vertex->next_ear || vertex->prev_ear)
  {
    PointDLL* head_reflex = tri->reflex;

    if (!is_ear(head_reflex, vertex))
    {
      // detach ear
      if (vertex == tri->ear)
      {
        tri->ear = tri->ear->next;
      }
      vertex->prev_ear->next_ear = vertex->next_ear;
      vertex->next_ear->prev_ear = vertex->prev_ear;
      vertex->next_ear           = 0;
      vertex->prev_ear           = 0;
    }
  }
  else if (vertex->next_reflex || vertex->prev_reflex)
  {

    if (is_convex(vertex))
    {

      // isn't reflex anymore
      PointDLL* next_reflex = 0;
      if (vertex->next_reflex)
      {
        vertex->next_reflex->prev_reflex = vertex->prev_reflex;
        vertex->prev_reflex->next_reflex = vertex->next_reflex;
        next_reflex                      = vertex->next_reflex;
        vertex->next_reflex              = 0;
        vertex->prev_reflex              = 0;
      }
      tri->reflex = next_reflex;

      // the thing is now convex
      PointDLL* conv = tri->convex;
      if (conv == 0)
      {
        vertex->next_convex = vertex;
        vertex->prev_convex = vertex;
        tri->convex         = vertex;
      }
      else
      {
        conv->next_convex->prev_convex = vertex;
        vertex->next_convex            = conv->next_convex;
        conv->next_convex              = vertex;
        vertex->prev_convex            = conv;
      }

      // check if it also became an ear
      if (is_ear(tri->reflex, vertex))
      {
        attach_ear(tri, vertex);
      }
    }
  }
  // otherwise convex and can become an ear
  else if (is_ear(tri->reflex, vertex))
  {
    attach_ear(tri, vertex);
  }
}

static inline void add_triangle(Triangle* triangle, PointDLL* ear)
{

  triangle->points[0] = ear->prev->point;
  triangle->points[1] = ear->point;
  triangle->points[2] = ear->next->point;
}

static inline void detach_ear_from_ears(Triangulation* tri)
{
  PointDLL* next_ear           = tri->ear->next_ear;
  next_ear->prev_ear           = tri->ear->prev_ear;
  tri->ear->prev_ear->next_ear = next_ear;
  tri->ear->next_ear           = 0;
  tri->ear->prev_ear           = 0;
  tri->ear                     = next_ear;
}

static inline void detach_ear_from_convex(Triangulation* tri)
{
  PointDLL* next_convex = 0;
  if (tri->ear->next_convex)
  {
    if (tri->ear == tri->convex)
    {
      tri->convex = tri->convex->next;
    }
    tri->ear->next_convex->prev_convex = tri->ear->prev_convex;
    next_convex                        = tri->ear->next_convex;
    tri->ear->next_convex              = 0;
  }
  if (tri->ear->prev_convex)
  {
    if (tri->ear == tri->convex)
    {
      tri->convex = tri->convex->prev;
    }
    tri->ear->prev_convex->next_convex = next_convex;
    tri->ear->prev_convex              = 0;
  }
}

static inline void detach_node(Triangulation* tri, PointDLL** _prev, PointDLL** _next)
{

  // test both adjacent
  PointDLL* prev = tri->ear->prev;
  PointDLL* next = tri->ear->next;

  prev->next     = next;
  next->prev     = prev;
  tri->ear->next = 0;
  tri->ear->prev = 0;
  detach_ear_from_convex(tri);
  detach_ear_from_ears(tri);

  *_prev = prev;
  *_next = next;
}

bool remove_vertex(Triangle* triangle, Triangulation* tri, u32& remaining)
{

  if (remaining >= 3)
  {
    add_triangle(triangle, tri->ear);
  }
  remaining--;
  if (remaining <= 2)
  {

    remaining = MAX(2, remaining);
    return true;
  }
  PointDLL *prev, *next;
  detach_node(tri, &prev, &next);

  test_vertex(prev, tri);
  test_vertex(next, tri);

  debug_points(tri);
  printf("Next in line: %c\n", translate(tri->ear->idx));
  return false;
}

// assumes that it is a simple polygon!
void triangulate_simple_via_ear_clipping(Triangle** out, u32& out_count, Vector2* v_points, u32 point_count)
{
  PointDLL* vertices;
  PointDLL* ear;

  // get_vertices(&ear, &vertices, v_points, point_count);
  // debug_points(vertices);

  Triangle* triangles = (Triangle*)sta_allocate_struct(Triangle, point_count - 2);
  out_count           = point_count - 2;
  u32 remaining       = point_count;
  u32 count           = 0;

  while (1)
  {
    // if (remove_vertex(&triangles[count++], vertices, &ear, remaining, point_count))
    // {
    // break;
    // }
  }

  *out = triangles;
}
