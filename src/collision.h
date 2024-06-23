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
struct Triangulation
{
  PointDLL* head;
  PointDLL* ear;
  PointDLL* convex;
  PointDLL* reflex;
  u32       count;
};

void debug_points(Triangulation * tri);
bool remove_vertex(Triangle* triangle, Triangulation* tri, u32& remaining);
void triangulate_simple_via_ear_clipping(Triangle** out, u32& out_count, Vector2* v_points, u32 point_count);
void get_vertices(Triangulation * tri, Vector2* v_points, u32 point_count);

#endif
