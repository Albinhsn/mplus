#ifndef STA_VECTOR
#define STA_VECTOR

#include "common.h"
class Mat33 {
public:
  union {
    float m[9];
    float rc[3][3];
  };

  float determinant();
  Mat33 inverse();
};
class Mat22 {
public:
  union {
    float m[4];
    float rc[2][2];
  };

  float determinant();
  Mat22 inverse();
};

class Vector2 {
public:
  Vector2() {
    this->x = 0;
    this->y = 0;
  }
  Vector2(float x, float y) {
    this->x = x;
    this->y = y;
  }
  float x;
  float y;
  float len();
  float dot(Vector2 v);
  float dot_perp(Vector2 v);
  Vector2 cross(Vector2 v);
  void scale(float s);
  Vector2 sub(Vector2 v);
  static Vector2 cross(Vector2 v0, Vector2 v1);
};

class Vector3 {
public:
  Vector3() {
    this->x = 0;
    this->y = 0;
    this->z = 0;
  }
  Vector3(f32 x, f32 y, f32 z) {
    this->x = x;
    this->y = y;
    this->z = z;
  }
  union {
    struct {
      float x;
      float y;
      float z;
    };
    float v[3];
  };

  float len();
  float dot(Vector3 v);
  Vector3 cross(Vector3 v);
  void scale(float s);
  Vector3 sub(Vector3 v);
  static Vector3 cross(Vector3 v0, Vector3 v1);
};

class Vector4 {
public:
  Vector4() {}
  Vector4(f32 x, f32 y, f32 z, f32 w) {
    this->x = x;
    this->y = y;
    this->z = z;
    this->w = w;
  }
  union {
    struct {
      float x;
      float y;
      float z;
      float w;
    };
    float v[4];
  };

  float len();
  float dot(Vector4 v);
  Vector4 cross(Vector4 v);
  void scale(float s);
  Vector4 sub(Vector4 v);
  static Vector4 cross(Vector4 v0, Vector4 v1);
};
class Mat44 {
public:
  union {
    float m[16];
    float rc[4][4];
  };

  void debug();
  float determinant();
  static Mat44 look_at(Vector3 x, Vector3 y, Vector3 z);
  Mat44 transpose();
  Mat44 mul(Mat44 m);
  Vector4 mul(Vector4 v);
  Mat44 inverse();
  void identity();
  Mat44 translate(Vector3 v);
  Mat44 scale(Vector3 v);
  Mat44 rotate_x(f32 r);
  Mat44 rotate_y(f32 r);
  Mat44 rotate_z(f32 r);
  void perspective(f32 fov, f32 screen_aspect, f32 screen_near,
                   f32 screen_depth);
  void orthographic(f32 screen_width, f32 screen_height, f32 screen_near,
                    f32 screen_depth);
};

typedef Vector2 Point2;
typedef Vector3 Point3;

struct Color {
  float r;
  float g;
  float b;
  float a;
};
struct ColorU8 {
  u8 r;
  u8 g;
  u8 b;
  u8 a;
};

class ConvexHull2D {
public:
  Point2 *points;
  int count;
  int cap;
  static ConvexHull2D create_andrew(Point2 *points, int point_count);
  static ConvexHull2D create_quick(Point2 *points, int point_count);
};

float orient2d(Point2 a, Point2 b, Point2 c);
float orient3d(Point3 a, Point3 b, Point3 c);
float in_circle2d(Point2 a, Point2 b, Point2 c, Point2 d);
float in_sphere(Point3 a, Point3 b, Point3 c, Point3 d, Point3 e);

#define BLACK                                                                  \
  { 0, 0, 0, 0 }
#define WHITE                                                                  \
  { 1.0f, 1.0f, 1.0f, 1.0f }

#endif
