#ifndef COLLISION_H
#define COLLISION_H
#include "common.h"
#include "vector.h"

struct Triangle
{
public:
  Triangle()
  {
  }
  Triangle(Vector2 v0, Vector2 v1, Vector2 v2)
  {
    points[0] = v0;
    points[1] = v1;
    points[2] = v2;
  }
  Vector2 points[3];
};
struct EarClippingNode
{
  EarClippingNode* prev;
  EarClippingNode* next;
  EarClippingNode* next_ear;
  EarClippingNode* prev_ear;
  EarClippingNode* next_convex;
  EarClippingNode* prev_convex;
  EarClippingNode* next_reflex;
  EarClippingNode* prev_reflex;
  Vector2   point;
  u32       idx;
};
struct EarClippingNodes
{
public:
  inline void detach_node(EarClippingNode** _prev, EarClippingNode** _next);
  inline void detach_ear_from_convex();
  inline void detach_ear_from_ears();
  void        test_vertex(EarClippingNode* vertex);
  void        attach_ear(EarClippingNode* vertex);
  EarClippingNode*   head;
  EarClippingNode*   ear;
  EarClippingNode*   convex;
  EarClippingNode*   reflex;
  u32         count;
};

void debug_points(EarClippingNodes* tri);
bool remove_vertex(Triangle* triangle, EarClippingNodes* tri);
void triangulate_simple_via_ear_clipping(Triangle** out, u32& out_count, Vector2* v_points, u32 point_count);
void triangulation_hole_via_ear_clipping(Vector2 ** vertices, u32 &out_count, Vector2** v_points, u32* point_count, u32 polygon_count);
void get_vertices(EarClippingNodes* tri, Vector2* v_points, u32 point_count);

void triangulate(Triangle* triangles, u32& triangle_count, Vector2** v_points, u32* v_counts, u32 count, u32 number_of_triangles);

#endif
