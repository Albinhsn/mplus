#include "collision.h"
#include "common.h"
#include "platform.h"
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdlib>

static inline bool compare_vertices(Vector2 v0, Vector2 v1)
{
  return compare_float(v0.x, v1.x) && compare_float(v0.y, v1.y);
}

inline float cross_2d(Point2 u, Point2 v)
{
  return u.y * v.x - u.x * v.y;
}

// checks clockwise
inline bool point_in_triangle_2d(Triangle t, Point2 p)
{
  if (cross_2d(p.sub(t.points[0]), t.points[1].sub(t.points[0])) > 0.0f)
  {
    return false;
  }
  if (cross_2d(p.sub(t.points[1]), t.points[2].sub(t.points[1])) > 0.0f)
  {
    return false;
  }
  if (cross_2d(p.sub(t.points[2]), t.points[0].sub(t.points[2])) > 0.0f)
  {
    return false;
  }
  return true;
}

inline float orient_2d(Vector2 v0, Vector2 v1, Vector2 p)
{
  return (v0.x - p.x) * (v1.y - p.y) - (v0.y - p.y) * (v1.x - p.x);
}

static bool is_convex(Vector2 v0, Vector2 v1, Vector2 v2)
{

  f32 v0v1 = orient_2d(v0, v1, v2);
  f32 v1v2 = orient_2d(v1, v2, v0);

  return v0v1 < 0 && v1v2 < 0;
}

static bool is_ear(Vector2 v0, Vector2 v1, Vector2 v2, Vector2* v, u32 v_count, u32 low, u32 mid, u32 high)
{
  Triangle t(v0, v1, v2);
  for (i32 i = 0; i < v_count; i++)
  {
    // only need to test reflex vertices?
    bool is_point_on_line = compare_vertices(v[i], v0) || compare_vertices(v[i], v1) || compare_vertices(v[i], v2);
    if (i != low && i != mid && i != high && point_in_triangle_2d(t, v[i]) && !is_point_on_line)
    {
      return false;
    }
  }
  return true;
}

static bool is_ear(EarClippingNode* reflex, EarClippingNode* vertex)
{
  Triangle tri   = {vertex->prev->point, vertex->point, vertex->next->point};
  u64      start = (u64)reflex;
  if (reflex)
  {
    do
    {
      bool is_point_on_line = compare_vertices(reflex->point, vertex->prev->point) || compare_vertices(reflex->point, vertex->point) || compare_vertices(reflex->point, vertex->next->point);
      if (!is_point_on_line && reflex != vertex->prev && reflex != vertex->next)
      {
        if (point_in_triangle_2d(tri, reflex->point))
        {
          return false;
        }
      }
      else
      {
      }
      reflex = reflex->next_reflex;
    } while ((u64)reflex != start && reflex);
  }
  return true;
}

static void get_earclipping_vertices(EarClippingNodes* tri, Vector2* v_points, u32 point_count)
{
  tri->head             = (EarClippingNode*)sta_allocate_struct(EarClippingNode, point_count);
  tri->count            = point_count;

  tri->ear              = 0;
  tri->convex           = 0;
  tri->reflex           = 0;

  EarClippingNode* curr = 0;
  for (i32 i = 0; i < point_count; i++)
  {
    tri->head[i].point = v_points[i];
    tri->head[i].next  = &tri->head[(i + 1) % point_count];
    tri->head[i].prev  = &tri->head[i - 1 < 0 ? 0 : i - 1];
    tri->head[i].idx   = i;
    curr               = &tri->head[i];
    curr->idx          = i;

    i32 i_low          = i - 1 < 0 ? point_count - 1 : i - 1;
    i32 i_high         = (i + 1) % point_count;
    curr->prev         = &tri->head[i_low];
    curr->next         = &tri->head[i_high];

    Vector2  v0        = v_points[i_low];
    Vector2  v1        = v_points[i];
    Vector2  v2        = v_points[i_high];
    Triangle t(v0, v1, v2);
    if (is_convex(v0, v1, v2))
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

      if (is_ear(v0, v1, v2, v_points, point_count, i_low, i, i_high))
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
      c                        = true;
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

  if (!e && tri->ear)
  {
    tri->ear->next_ear = tri->ear;
    tri->ear->prev_ear = tri->ear;
  }
  if (!c && tri->convex)
  {
    tri->convex->next_convex = tri->convex;
    tri->convex->prev_convex = tri->convex;
  }
  if (!r && tri->reflex)
  {
    tri->reflex->next_reflex = tri->reflex;
    tri->reflex->prev_reflex = tri->reflex;
  }
}

void EarClippingNodes::attach_ear(EarClippingNode* vertex)
{
  EarClippingNode* head_ear = this->ear;
  if (head_ear == 0)
  {
    vertex->next_ear = vertex;
    vertex->prev_ear = vertex;
    this->ear        = vertex;
  }
  else
  {
    if (head_ear->next_ear)
    {
      head_ear->next_ear->prev_ear = vertex;
    }
    vertex->next_ear   = head_ear->next_ear;
    head_ear->next_ear = vertex;
    vertex->prev_ear   = head_ear;
  }
}

void EarClippingNodes::test_vertex(EarClippingNode* vertex)
{

  // might not be an ear, but still always convex?
  if (vertex->next_ear || vertex->prev_ear)
  {
    EarClippingNode* head_reflex = this->reflex;

    if (!is_ear(head_reflex, vertex))
    {
      // detach ear
      if (vertex == this->ear)
      {
        this->ear = this->ear->next_ear;
      }
      vertex->prev_ear->next_ear = vertex->next_ear;
      vertex->next_ear->prev_ear = vertex->prev_ear;
      vertex->next_ear           = 0;
      vertex->prev_ear           = 0;
    }
  }
  else if (vertex->next_reflex || vertex->prev_reflex)
  {

    if (is_convex(vertex->prev->point, vertex->point, vertex->next->point))
    {

      // isn't reflex anymore
      EarClippingNode* next_reflex = 0;
      if (vertex->next_reflex)
      {
        vertex->next_reflex->prev_reflex = vertex->prev_reflex;
        vertex->prev_reflex->next_reflex = vertex->next_reflex;
        next_reflex                      = vertex->next_reflex;
        vertex->next_reflex              = 0;
        vertex->prev_reflex              = 0;
      }
      this->reflex = next_reflex;

      // the thing is now convex
      EarClippingNode* conv = this->convex;
      if (conv == 0)
      {
        vertex->next_convex = vertex;
        vertex->prev_convex = vertex;
        this->convex        = vertex;
      }
      else
      {
        conv->next_convex->prev_convex = vertex;
        vertex->next_convex            = conv->next_convex;
        conv->next_convex              = vertex;
        vertex->prev_convex            = conv;
      }

      // check if it also became an ear
      if (is_ear(this->reflex, vertex))
      {
        this->attach_ear(vertex);
      }
    }
  }
  // otherwise convex and can become an ear
  else if (is_ear(this->reflex, vertex))
  {
    assert(vertex->next_convex && vertex->prev_convex && "ISN'T CONVEX?");
    this->attach_ear(vertex);
  }
}

static inline void add_triangle(Triangle* triangle, EarClippingNode* ear)
{

  triangle->points[0] = ear->prev->point;
  triangle->points[1] = ear->point;
  triangle->points[2] = ear->next->point;
}

inline void EarClippingNodes::detach_ear_from_ears()
{
  EarClippingNode* next_ear = this->ear->next_ear;
  if (next_ear)
  {
    next_ear->prev_ear            = this->ear->prev_ear;
    this->ear->prev_ear->next_ear = next_ear;

    this->ear->next_ear           = 0;
    this->ear->prev_ear           = 0;
    this->ear                     = next_ear;
  }
  else
  {
    this->ear = 0;
  }
}

inline void EarClippingNodes::detach_ear_from_convex()
{
  EarClippingNode* next_convex = 0;
  if (this->ear->next_convex)
  {
    if (this->ear == this->convex)
    {
      this->convex = this->convex->next_convex;
    }
    this->ear->next_convex->prev_convex = this->ear->prev_convex;
    next_convex                         = this->ear->next_convex;
    this->ear->next_convex              = 0;
  }
  if (this->ear->prev_convex)
  {
    if (this->ear == this->convex)
    {
      this->convex = this->convex->prev;
    }
    this->ear->prev_convex->next_convex = next_convex;
    this->ear->prev_convex              = 0;
  }
}

inline void EarClippingNodes::detach_node(EarClippingNode** _prev, EarClippingNode** _next)
{

  // test both adjacent
  EarClippingNode* prev = this->ear->prev;
  EarClippingNode* next = this->ear->next;

  prev->next            = next;
  next->prev            = prev;
  this->ear->next       = 0;
  this->ear->prev       = 0;
  this->detach_ear_from_convex();
  this->detach_ear_from_ears();

  *_prev = prev;
  *_next = next;
}

static bool remove_vertex(Triangle* triangle, EarClippingNodes* tri)
{

  add_triangle(triangle, tri->ear);
  EarClippingNode *prev, *next;
  tri->detach_node(&prev, &next);

  tri->test_vertex(prev);
  tri->test_vertex(next);

  EarClippingNode* head  = tri->ear;
  u64              start = (u64)head;
  assert(tri->ear != 0 && "Out of ears?");
  return false;
}

struct PolygonMaxXPair
{
  Vector2* max_x;
  Vector2* points;
  u32      point_count;
  u32      max_x_idx;
};

struct VertexDLL
{
  VertexDLL* prev;
  VertexDLL* next;
  Vector2    point;
  u32        idx;
};

inline int sort_polygon_min_x_pair(const void* _a, const void* _b)
{
  f32 res = ((PolygonMaxXPair*)_a)->max_x->x - ((PolygonMaxXPair*)_b)->max_x->x;
  if (res == 0)
  {
    return 0;
  }
  return res > 0 ? -1 : 1;
}

static inline void insert_vertex_prior(VertexDLL* head, VertexDLL* p, u32& count, Vector2 v)
{
  VertexDLL* new_h = &head[count];
  new_h->prev      = p->prev;
  new_h->next      = p;
  new_h->idx       = count;
  ++count;
  p->prev->next = new_h;
  p->prev       = new_h;
  new_h->point  = v;
}

static void find_bridge(VertexDLL* outer, u32& outer_count, PolygonMaxXPair* inner)
{

  Vector2*   C         = inner->max_x;
  VertexDLL* head      = outer;
  u64        start     = (u64)head;

  bool       is_vertex = false;
  VertexDLL* h         = 0;
  Vector2    I;
  f32        min_dist = FLT_MAX;
  Vector2    n        = {0, 1};
  do
  {
    Vector2 A = head->point;
    Vector2 B = head->next->point;
    if (compare_float(A.x, B.x) && ((A.y <= C->y && B.y >= C->y) || (A.y >= C->y && B.y <= C->y)))
    {
      f32 dist = B.x - C->x;
      if (dist > 0 && min_dist > dist)
      {
        h    = head;
        dist = min_dist;
        I    = Vector2(B.x, C->y);
        // robust?
        is_vertex = compare_float(A.y, C->y) || compare_float(B.y, C->y);
      }
    }
    else if ((A.y >= C->y && B.y <= C->y) || (B.y >= C->y && A.y <= C->y) && (A.x >= C->x || B.x >= C->x))
    {
      Vector2 s            = B.sub(A);
      f32     t            = n.dot(C->sub(A)) / n.dot(B.sub(A));
      Vector2 intersection = {A.x + s.x * t, A.y + s.y * t};
      if (0 <= t && t <= 1)
      {
        f32 dist = intersection.x - C->x;
        if (min_dist > dist)
        {
          h         = head;
          dist      = min_dist;
          I         = intersection;
          is_vertex = t == 0 || t == 1;
        }
      }
    }

    head = head->next;
  } while (start != (u64)head);

  assert(h && "Didn't find any point!");

  if (!is_vertex)
  {
    Vector2*   M = C;
    Vector2    P = h->point.x > h->next->point.x ? h->point : h->next->point;
    Triangle   MIP(*M, I, P);

    VertexDLL* min_angle = 0;
    f32        angle     = 0;
    head                 = outer;
    start                = (u64)head;
    do
    {

      if (!is_convex(head->prev->point, head->point, head->next->point) && head != h)
      {
        if (point_in_triangle_2d(MIP, head->point))
        {
          Vector2 MI = I.sub(*M);
          Vector2 MR = head->point.sub(*M);
          f32     a  = std::abs(acosf(MI.dot(MR) / (MI.len() * MR.len())));
          if (a < angle)
          {
            angle     = a;
            min_angle = head;
          }
        }
      }

      head = head->next;
    } while (start != (u64)head);

    if (min_angle)
    {
      h = min_angle;
    }
  }

  insert_vertex_prior(outer, h, outer_count, h->point);
  h->prev->idx  = h->idx;

  i32 start_idx = inner->max_x_idx;
  do
  {
    insert_vertex_prior(outer, h, outer_count, inner->points[start_idx]);
    start_idx = (start_idx + 1) % inner->point_count;
  } while (start_idx != inner->max_x_idx);
  insert_vertex_prior(outer, h, outer_count, inner->points[start_idx]);
}

// requires the ordering of the outer and inner vertices to be opposite
void create_simple_polygon_from_polygon_with_holes(Vector2** out, u32& out_count, Vector2** v_points, u32* point_count, u32 polygon_count)
{

  PolygonMaxXPair pairs[polygon_count];
  u32             total_count = 0;
  for (u32 i = 0; i < polygon_count; i++)
  {
    Vector2* points      = v_points[i];
    u32      count       = point_count[i];
    pairs[i].point_count = count;
    pairs[i].points      = points;
    pairs[i].max_x       = &points[0];
    pairs[i].max_x_idx   = 0;

    for (u32 j = 1; j < count; j++)
    {
      if (points[j].x > pairs[i].max_x->x)
      {
        pairs[i].max_x     = &points[j];
        pairs[i].max_x_idx = j;
      }
    }
    total_count += count;
  }
  qsort(pairs, polygon_count, sizeof(PolygonMaxXPair), sort_polygon_min_x_pair);
  for (u32 i = 0; i < polygon_count; i++)
  {
    v_points[i]    = pairs[i].points;
    point_count[i] = pairs[i].point_count;
  }

  VertexDLL*      vertices   = (VertexDLL*)sta_allocate_struct(VertexDLL, total_count + (polygon_count - 1) * 2);
  PolygonMaxXPair first_pair = pairs[0];
  u32             count      = first_pair.point_count;
  for (i32 i = 0; i < count; i++)
  {
    VertexDLL* v = &vertices[i];
    v->point     = first_pair.points[i];
    v->next      = &vertices[(i + 1) % count];
    v->prev      = &vertices[i - 1 < 0 ? count - 1 : i - 1];
    v->idx       = i;
  }

  for (u32 i = 1; i < polygon_count; i++)
  {
    find_bridge(vertices, count, &pairs[i]);
    u64        start = (u64)vertices;
    VertexDLL* head  = vertices;
    u32        c     = 0;
    do
    {
      head = head->next;
      c++;
    } while (start != (u64)head);
  }

  out_count        = total_count + (polygon_count - 1) * 2;
  Vector2*   v     = (Vector2*)sta_allocate_struct(Vector2, out_count);
  u64        start = (u64)vertices;
  VertexDLL* head  = vertices;
  u64        i     = 0;
  do
  {
    v[i++] = head->point;
    head   = head->next;
  } while (start != (u64)head);

  assert(i == count && "Didn't fill?");
  assert(count == out_count && "Didn't equal in size?");

  *out = v;

  sta_deallocate(vertices, sizeof(VertexDLL) * (total_count + polygon_count - 1));
}

void triangulate_earclipping(Triangle* triangles, u32& triangle_count, Vector2** v_points, u32* v_counts, u32 count, u32 number_of_triangles)
{

  EarClippingNodes tri[count];

  for (u32 i = 0; i < count; i++)
  {
    get_earclipping_vertices(&tri[i], v_points[i], v_counts[i]);
    triangle_count += v_counts[i] - 2;
  }
  u32 tot = 0;
  for (u32 i = 0; i < count; i++)
  {
    for (int j = 0; j < v_counts[i] - 2; j++)
    {
      assert(tot + j < number_of_triangles && "OVERFLOW?");
      remove_vertex(&triangles[tot + j], &tri[i]);
    }
    tot += v_counts[i] - 2;
  }
}
