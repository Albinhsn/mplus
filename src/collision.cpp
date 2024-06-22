#include "collision.h"
#include <cassert>


inline float cross_2d(Point2 u, Point2 v)
{
  return u.y * v.x - u.x * v.y;
}

inline bool point_in_triangle_2d(Triangle t, Point2 p)
{
  if (cross_2d(p.sub(t.points[0]), t.points[1].sub(t.points[0])) < 0.0f)
  {
    return false;
  }
  if (cross_2d(p.sub(t.points[1]), t.points[2].sub(t.points[1])) < 0.0f)
  {
    return false;
  }
  if (cross_2d(p.sub(t.points[2]), t.points[0].sub(t.points[2])) < 0.0f)
  {
    return false;
  }
  return true;
}

inline float orient_2d(Vector2 v0, Vector2 v1, Vector2 p)
{
  return (v0.x - p.x) * (v1.y - p.y) - (v0.y - p.y) * (v1.x - p.x);
}
struct PointDLL
{
  PointDLL* prev;
  PointDLL* next;
  PointDLL* next_ear;
  PointDLL* prev_ear;
  PointDLL* next_convex;
  PointDLL* prev_convex;
  PointDLL* next_reflex;
  PointDLL* prev_reflex;
  Vector2   point;
  u32       idx;
};

static inline void insert_node_cyclical(PointDLL* list, Vector2 v, u32 idx, u32 point_count)
{
  list[idx].point = v;
  list[idx].next  = &list[(idx + 1) % point_count];
  list[idx].prev  = &list[idx - 1 < 0 ? 0 : idx - 1];
  list[idx].idx   = idx;
}

void get_vertices(PointDLL** head_ear, PointDLL** _vertices, Vector2* v_points, u32 point_count)
{
  PointDLL* vertices    = (PointDLL*)sta_allocate_struct(PointDLL, point_count);

  PointDLL* tail_ear    = 0;
  PointDLL* tail_convex = 0;
  PointDLL* tail_reflex = 0;

  PointDLL* curr        = 0;
  for (i32 i = 0; i < point_count; i++)
  {
    insert_node_cyclical(vertices, v_points[i], i, point_count);
    curr         = &vertices[i];
    curr->idx    = i;

    i32 i_low    = i - 1 < 0 ? point_count - 1 : i - 1;
    i32 i_high   = (i + 1) % point_count;
    curr->prev   = &vertices[i_low];
    curr->next   = &vertices[i_high];

    Vector2 v0   = v_points[i_low];
    Vector2 v1   = v_points[i];
    Vector2 v2   = v_points[i_high];
    f32     v0v1 = orient_2d(v0, v1, v2);
    f32     v1v2 = orient_2d(v1, v2, v0);

    // test if any other vertex is inside the triangle from these vertices

    Triangle t(v0, v1, v2);

    if (v0v1 > 0 && v1v2 > 0)
    {
      if (tail_convex != 0)
      {
        tail_convex->next_convex = curr;
        curr->prev_convex        = tail_convex;
        tail_convex              = curr;
      }
      else
      {
        tail_convex = curr;
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
        if (tail_ear != 0)
        {
          tail_ear->next_ear = curr;
          curr->prev_ear     = tail_ear;
          tail_ear           = curr;
        }
        else
        {
          tail_ear = curr;
        }
      }
    }
    else
    {
      if (tail_reflex != 0)
      {
        tail_reflex->next_reflex = curr;
        curr->prev_reflex        = tail_reflex;
        tail_reflex              = curr;
      }
      else
      {
        tail_reflex = curr;
      }
    }
  }

  for (u32 i = 0; i < point_count; i++)
  {
    if (vertices[i].next_ear)
    {
      *head_ear            = &vertices[i];
      tail_ear->next_ear   = &vertices[i];
      vertices[i].prev_ear = tail_ear;
      break;
    }
  }

  *_vertices = vertices;
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
  assert(!"Found no convex vertex?");
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
    if ((vertices[i].next_ear || vertices[i].prev_ear) && vertices != current_ear)
    {
      return &vertices[i];
    }
  }
  assert(!"Found no ear vertex?");
}

void test_vertex(PointDLL* vertex, PointDLL* vertices, u32 point_count, PointDLL* current_ear)
{

  // might not be an ear, but still always convex?
  if (vertex->next_ear || vertex->prev_ear)
  {
    // test against all reflex vertices
    Vector2   v0          = vertex->prev->point;
    Vector2   v1          = vertex->point;
    Vector2   v2          = vertex->next->point;
    Triangle  triangle    = {v0, v1, v2};

    PointDLL* head_reflex = get_reflex_vertex(vertices, point_count, vertex);

    bool      found       = false;
    while (head_reflex)
    {
      if (head_reflex != vertex->prev && head_reflex != vertex->next && point_in_triangle_2d(triangle, head_reflex->point))
      {
        found = true;
        break;
      }
      head_reflex = head_reflex->next_reflex;
    }
    if (found)
    {
      vertex->prev_ear->next_ear = vertex->next_ear;
      vertex->next_ear->prev_ear = vertex->prev_ear;
      vertex->next_ear           = 0;
      vertex->prev_ear           = 0;
    }
  }
  else if (vertex->next_reflex || vertex->prev_reflex)
  {
    // might be convex and might be ear
    Vector2 v0   = vertex->prev->point;
    Vector2 v1   = vertex->point;
    Vector2 v2   = vertex->next->point;
    f32     v0v1 = orient_2d(v0, v1, v2);
    f32     v1v2 = orient_2d(v1, v2, v0);

    // test if any other vertex is inside the triangle from these vertices

    if (v0v1 > 0 && v1v2 > 0)
    {
      if (vertex->next_reflex)
      {
        vertex->next_reflex->prev_reflex = vertex->prev_reflex;
        vertex->next_reflex              = 0;
      }
      if (vertex->prev_reflex)
      {
        vertex->prev_reflex->next_reflex = vertex->next_reflex;
        vertex->prev_reflex              = 0;
      }

      // the thing is now convex
      PointDLL* convex = get_convex_vertex(vertices, point_count);
      if (convex->next_convex)
      {
        convex->next_convex->prev_convex = vertex;
        vertex->next_convex              = convex->next_convex;
        convex->next_convex              = vertex;
        vertex->prev_convex              = convex;
      }
      if (convex->prev_convex)
      {
        convex->prev_convex->next_convex = vertex;
        vertex->prev_convex              = convex->prev_convex;
        convex->prev_convex              = vertex;
        vertex->next_convex              = convex;
      }

      bool      found  = false;
      PointDLL* reflex = get_reflex_vertex(vertices, point_count, vertex);
      Triangle  t      = {v0, v1, v2};
      while (reflex)
      {
        if (reflex != vertex->prev && reflex != vertex->next && point_in_triangle_2d(t, reflex->point))
        {
          found = true;
          break;
        }
        reflex = reflex->next_reflex;
      }

      if (!found)
      {
        PointDLL* ear = get_ear_vertex(vertices, point_count, current_ear);
        if (ear->next_ear)
        {
          ear->next_ear->prev_ear = vertex;
          vertex->next_ear        = ear->next_ear;
          ear->next_ear           = vertex;
          vertex->prev_ear        = ear;
        }
        else
        {
          ear->next_ear    = vertex;
          ear->prev_ear    = vertex;
          vertex->prev_ear = ear;
          vertex->next_ear = ear;
        }
      }
    }
  }
  // otherwise convex and can become an ear
  else
  {

    Vector2   v0     = vertex->prev->point;
    Vector2   v1     = vertex->point;
    Vector2   v2     = vertex->next->point;
    bool      found  = false;
    PointDLL* reflex = get_reflex_vertex(vertices, point_count, vertex);
    Triangle  t      = {v0, v1, v2};
    while (reflex)
    {
      if (reflex != vertex->prev && reflex != vertex->next && point_in_triangle_2d(t, reflex->point))
      {
        found = true;
        break;
      }
      reflex = reflex->next_reflex;
    }

    if (!found)
    {
      PointDLL* ear = get_ear_vertex(vertices, point_count, current_ear);
      if (ear->next_ear)
      {
        ear->next_ear->prev_ear = vertex;
        vertex->next_ear        = ear->next_ear;
        ear->next_ear           = vertex;
        vertex->prev_ear        = ear;
      }
      else
      {
        ear->next_ear    = vertex;
        ear->prev_ear    = vertex;
        vertex->prev_ear = ear;
        vertex->next_ear = ear;
      }
    }
  }
}

int translate(PointDLL* point)
{
  return !point ? -1 : point->idx;
}

void debug_points(PointDLL* points, u32 point_count)
{
  for (u32 i = 0; i < point_count; i++)
  {
    printf("%d:\n", points[i].idx);
    printf("Chain:  %d\t%d\n", translate(points[i].prev), translate(points[i].next));
    printf("Ear:    %d\t%d\n", translate(points[i].prev_ear), translate(points[i].next_ear));
    printf("Convex: %d\t%d\n", translate(points[i].prev_convex), translate(points[i].next_convex));
    printf("Reflex: %d\t%d\n", translate(points[i].prev_reflex), translate(points[i].next_reflex));
    printf("-\n");
  }
  printf("-----\n");
}

// assumes that it is a simple polygon!
void triangulate_simple_via_ear_clipping(Triangle** out, u32& out_count, Vector2* v_points, u32 point_count)
{
  // vertices   cyclical
  // convex     linear
  // reflex     linear
  // ears       cyclical

  PointDLL* vertices;
  PointDLL* ear;

  get_vertices(&ear, &vertices, v_points, point_count);

  Triangle* triangles = (Triangle*)sta_allocate_struct(Triangle, point_count - 2);
  out_count           = point_count - 2;
  u32 remaining       = point_count;

  while (1)
  {

    // add the removed ear to something
    Triangle* triangle  = &triangles[point_count - remaining];
    triangle->points[0] = ear->prev->point;
    triangle->points[1] = ear->point;
    triangle->points[2] = ear->next->point;
    remaining--;
    if (remaining <= 3)
    {
      // add last triangle
      Triangle* triangle  = &triangles[point_count - 3];
      ear                 = ear->next->next;
      triangle->points[0] = ear->prev->point;
      triangle->points[1] = ear->point;
      triangle->points[2] = ear->next->point;
      break;
    }

    if (ear->next_convex)
    {
      ear->next_convex->prev_convex = ear->prev_convex;
      ear->next_convex              = 0;
    }
    if (ear->prev_convex)
    {
      ear->prev_convex->next_convex = ear->next_convex;
      ear->prev_convex              = 0;
    }
    // test both adjacent
    PointDLL* prev = ear->prev;
    PointDLL* next = ear->next;

    // remove the vertex from vertices
    ear->next->prev = ear->prev;
    ear->prev->next = ear->next;

    test_vertex(prev, vertices, point_count, ear);
    test_vertex(next, vertices, point_count, ear);

    if (ear == ear->next_ear)
    {
      assert(!"Shouldn't happen!");
    }

    // get the next ear
    PointDLL* next_ear = ear->next_ear;

    next_ear->prev_ear = ear->prev_ear;
    if (ear->prev_ear)
    {
      ear->prev_ear->next_ear = next_ear;
    }

    ear->next_ear = 0;
    ear->prev_ear = 0;
    ear           = next_ear;
  }

  *out = triangles;
}
