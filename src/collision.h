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

void triangulate_simple_via_ear_clipping(Triangle** out, u32& out_count, Vector2* v_points, u32 point_count);

#endif
