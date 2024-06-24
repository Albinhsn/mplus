#include "collision.h"
#include "common.h"
#include "platform.h"
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdlib>

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
int translate(EarClippingNode* point)
{
  return !point ? -1 : point->idx;
}

char translate(int i)
{
  char letters[] = "abcdefghijklmnopqrstuvwz123456789!\"#¤%&/()=+-?<>,;.:-_´^'*¨";
  assert(i <= ArrayCount(letters));
  return letters[i];
}

void debug_points(EarClippingNodes* tri)
{
  for (u32 i = 0; i < tri->count; i++)
  {
    EarClippingNode* head = &tri->head[i];
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

void get_vertices(EarClippingNodes* tri, Vector2* v_points, u32 point_count)
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

    Vector2 v0         = v_points[i_low];
    Vector2 v1         = v_points[i];
    Vector2 v2         = v_points[i_high];
    f32     v0v1       = orient_2d(v0, v1, v2);
    f32     v1v2       = orient_2d(v1, v2, v0);

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

static bool is_ear(EarClippingNode* reflex, EarClippingNode* vertex)
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

static bool is_convex(Vector2 v0, Vector2 v1, Vector2 v2)
{

  f32 v0v1 = orient_2d(v0, v1, v2);
  f32 v1v2 = orient_2d(v1, v2, v0);

  return v0v1 < 0 && v1v2 < 0;
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
    head_ear->next_ear->prev_ear = vertex;
    vertex->next_ear             = head_ear->next_ear;
    head_ear->next_ear           = vertex;
    vertex->prev_ear             = head_ear;
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
        this->ear = this->ear->next;
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
  EarClippingNode* next_ear     = this->ear->next_ear;
  next_ear->prev_ear            = this->ear->prev_ear;
  this->ear->prev_ear->next_ear = next_ear;
  this->ear->next_ear           = 0;
  this->ear->prev_ear           = 0;
  this->ear                     = next_ear;
}

inline void EarClippingNodes::detach_ear_from_convex()
{
  EarClippingNode* next_convex = 0;
  if (this->ear->next_convex)
  {
    if (this->ear == this->convex)
    {
      this->convex = this->convex->next;
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

bool remove_vertex(Triangle* triangle, EarClippingNodes* tri, u32& remaining)
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
  EarClippingNode *prev, *next;
  tri->detach_node(&prev, &next);

  tri->test_vertex(prev);
  tri->test_vertex(next);

  debug_points(tri);
  printf("Next in line: %c\n", translate(tri->ear->idx));
  return false;
}

// assumes that it is a simple polygon!
void triangulate_simple_via_ear_clipping(Triangle** out, u32& out_count, Vector2* v_points, u32 point_count)
{
  EarClippingNode* vertices;
  EarClippingNode* ear;

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

  // 1. Search the inner polygon for vertex M of maximum x-value
  Vector2* C = inner->max_x;

  // 2. Intersect the ray M +t(1,0) with all directed edges (Vi, Vi+1) of the outer polygon for which M is to the left of the line containing the edge.
  //    Moreover , the intersection tests need only involve directed edges for which Vi is below (or on) the ray and Vi+1 is above (or on) the ray.
  //    This is essential when the polygon has multiple holes and one or more holes have already had bridges inserted.
  //    Let I be the closest visible point to M on the ray. The implementation keeps track of this by monitoring the t-value in search of the smallest
  //    positive value for those edges in the intersection tests.
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
    if (B.y < C->y && A.y > C->y)
    {
      Vector2 s = B.sub(A);
      // t = n * (C-A)/(n * (B - A))
      f32     t            = n.dot(C->sub(A)) / n.dot(B.sub(A));
      Vector2 intersection = {A.x + s.x * t, A.y + s.y * t};
      if (0 <= t && t <= 1)
      {
        printf("Found (%f,  %f) %f\n", intersection.x, intersection.y, t);
        f32 dist = intersection.x - head->point.x;
        if (min_dist > dist)
        {
          h    = head;
          dist = min_dist;
          I    = intersection;
          // robust?
          is_vertex = t == 0 || t == 1;
        }
      }
    }

    head = head->next;
  } while (start != (u64)head);

  assert(h && "Didn't find any point!");

  // 3. If I is a vertex of the outer polygon, then M and I are mutually visible and the algorithm terminates.
  if (!is_vertex)
  {
    Vector2* M = C;
    // 4. Otherwise, I is an interior point of the edge (Vi, Vi+1). Select P to be the end point of maximum x-value for this edge
    Vector2  P = h->point.x > h->next->point.x ? h->point : h->next->point;
    Triangle MIP(*M, I, P);

    // 5. Search the reflex vertices of the outer polygon, not including P if it happens to be reflex. If all of those vertices are strictly outside triangle (M,I,P), then
    //    M and P are mutually visible and the algorithm terminates.
    // 6. Otherwise, at least one reflex vertex lies in (M,I,P). Search for the reflex R that minimizes the angle between (M,I) and (M,R);
    //    then M and R are mutually visible and the algorithm terminates.
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
          printf("Found reflex inside!\n");
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

  // h is the vertex we insert into
  // insert h itself
  insert_vertex_prior(outer, h, outer_count, h->point);
  printf("INTERSECTION POINT: %d\n", h->idx);
  h->prev->idx = h->idx;

  // insert the intersection point and walk until we reach back to it, then insert it again
  u32 start_idx = inner->max_x_idx;
  do
  {
    insert_vertex_prior(outer, h, outer_count, inner->points[start_idx]);
    start_idx = (start_idx + 1) % inner->point_count;
  } while (start_idx != inner->max_x_idx);
  insert_vertex_prior(outer, h, outer_count, inner->points[start_idx]);
}

// requires the ordering of the outer and inner vertices to be opposite
void triangulation_hole_via_ear_clipping(Vector2** out, u32& out_count, Vector2** v_points, u32* point_count, u32 polygon_count)
{

  // sort the polygons by their x-value
  PolygonMaxXPair pairs[polygon_count];
  u32             total_count = 0;
  for (u32 i = 0; i < polygon_count; i++)
  {
    Vector2* points      = v_points[i];
    u32      count       = point_count[i];
    pairs[i].point_count = count;
    pairs[i].points      = points;

    pairs[i].max_x       = &points[0];
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

  VertexDLL*      concatenated_polygon = (VertexDLL*)sta_allocate_struct(VertexDLL, total_count + (polygon_count - 1) * 2);
  PolygonMaxXPair first_pair           = pairs[0];
  u32             count                = first_pair.point_count;
  for (i32 i = 0; i < count; i++)
  {
    VertexDLL* v = &concatenated_polygon[i];
    v->point     = first_pair.points[i];
    v->next      = &concatenated_polygon[(i + 1) % count];
    v->prev      = &concatenated_polygon[i - 1 < 0 ? count - 1 : i - 1];
    v->idx       = i;
  }

  for (u32 i = 1; i < polygon_count; i++)
  {
    find_bridge(concatenated_polygon, count, &pairs[i]);
    u64        start = (u64)concatenated_polygon;
    VertexDLL* head  = concatenated_polygon;
    do
    {
      printf("%d (%f, %f)\n", head->idx, head->point.x, head->point.y);
      head = head->next;
    } while (start != (u64)head);
    printf("-\n");
  }

  out_count        = total_count + (polygon_count - 1) * 2;
  Vector2*   v     = (Vector2*)sta_allocate_struct(Vector2, out_count);
  u64        start = (u64)concatenated_polygon;
  VertexDLL* head  = concatenated_polygon;
  u64        i     = 0;
  do
  {
    v[i] = head->point;
    head = head->next;
    i++;
  } while (start != (u64)head);

  assert(i == count && "Didn't fill?");
  assert(count == out_count && "Didn't equal in size?");

  *out = v;

  sta_deallocate(concatenated_polygon, sizeof(VertexDLL) * (total_count + polygon_count - 1));
}
