#ifndef STA_VECTOR
#define STA_VECTOR

#include "common.h"
#include <cmath>
#include <cstdio>
#include <cstring>

typedef class Mat44 Mat44;
struct Quaternion
{
public:
  void debug()
  {
    printf("%f %f %f %f\n", x, y, z, w);
  }
  Quaternion()
  {
    this->x = 0;
    this->y = 0;
    this->z = 0;
    this->w = 0;
  }
  Quaternion(float x, float y, float z, float w)
  {
    this->x = x;
    this->y = y;
    this->z = z;
    this->w = w;
  }
  static Quaternion from_mat(Mat44 m);
  static Quaternion interpolate_linear(Quaternion q0, Quaternion q1, f32 t);
  static Quaternion interpolate_slerp(Quaternion q0, Quaternion q1, f32 t);
  f32               x, y, z, w;
};

class Mat43
{
};

class Mat33
{
public:
  union
  {
    float m[9];
    float rc[3][3];
  };

  float determinant();
  Mat33 inverse();
};
class Mat22
{
public:
  union
  {
    float m[4];
    float rc[2][2];
  };

  float determinant();
  Mat22 inverse();
};

class Vector2
{
public:
  Vector2()
  {
    this->x = 0;
    this->y = 0;
  }
  Vector2(float x, float y)
  {
    this->x = x;
    this->y = y;
  }
  void debug()
  {
    printf("%f %f\n", x, y);
  }
  float x;
  float y;
  float len()
  {
    return sqrtf(this->x * this->x + this->y * this->y);
  }
  void           normalize();
  float          dot(Vector2 v);
  float          dot_perp(Vector2 v);
  Vector2        cross(Vector2 v);
  void           scale(float s);
  Vector2        sub(Vector2 v);
  static Vector2 cross(Vector2 v0, Vector2 v1);
};

class Vector3
{
public:
  Vector3()
  {
    this->x = 0;
    this->y = 0;
    this->z = 0;
  }
  Vector3(f32 x, f32 y, f32 z)
  {
    this->x = x;
    this->y = y;
    this->z = z;
  }
  union
  {
    struct
    {
      float x;
      float y;
      float z;
    };
    float v[3];
  };

  void debug()
  {
    printf("%f %f %f\n", x, y, z);
  }
  float          len();
  float          dot(Vector3 v);
  Vector3        cross(Vector3 v);
  void           scale(float s);
  void           normalize();
  Vector3        sub(Vector3 v);
  static Vector3 cross(Vector3 v0, Vector3 v1);
};

class Vector4
{
public:
  Vector4()
  {
  }
  Vector4(f32 x, f32 y, f32 z, f32 w)
  {
    this->x = x;
    this->y = y;
    this->z = z;
    this->w = w;
  }
  Vector3 project()
  {
    Vector3 out = Vector3(this->x, this->y, this->z);
    out.scale(1.0f / this->w);
    return out;
  }
  void debug()
  {
    printf("%f %f %f %f\n", x, y, z, w);
  }
  union
  {
    struct
    {
      float x;
      float y;
      float z;
      float w;
    };
    float v[4];
  };

  float          len();
  float          dot(Vector4 v);
  Vector4 mul(Mat44 m);
  Vector4        cross(Vector4 v);
  void           scale(float s);
  Vector4        sub(Vector4 v);
  static Vector4 cross(Vector4 v0, Vector4 v1);
};
class Mat44
{
public:
  Mat44()
  {
    memset(&m[0], 0, sizeof(float) * 16);
  }
  union
  {
    float m[16];
    float rc[4][4];
  };

  void clear()
  {
    memset(&this->m[0], 0, 16 * 4);
  }
  void         debug();
  float        determinant();
  static Mat44 look_at(Vector3 c, Vector3 l, Vector3 u_prime);
  static Mat44 create_translation(Vector3 t);
  static Mat44 create_translation(f32 t[3]);
  static Mat44 create_rotation(Quaternion q);
  static Mat44 create_rotation2(Quaternion q);
  static Mat44 create_rotation(f32 q[4]);
  static Mat44 create_scale(Vector3 s);
  static Mat44 create_scale(f32 s[3]);
  static Mat44 orthographic(f32 left, f32 right, f32 bot, f32 top, f32 near, f32 far);
  void         transpose();
  Mat44        mul(Mat44 m);
  Vector4      mul(Vector4 v);
  Mat44        inverse();
  void         identity();
  Mat44        translate(Vector3 v);
  Mat44        scale(Vector3 v);
  Mat44        scale(Vector4 v);
  Mat44        scale(f32 scale);
  Mat44        rotate_x(f32 r);
  Mat44        rotate_y(f32 r);
  Mat44        rotate_z(f32 r);
  Mat44        rotate(Quaternion q);
  void         perspective(f32 fov, f32 screen_aspect, f32 screen_near, f32 screen_depth);
};

typedef Vector2 Point2;
typedef Vector3 Point3;

struct Color
{
public:
  Color()
  {
  }
  Color(f32 r, f32 g, f32 b, f32 a)
  {
    this->r = r;
    this->g = g;
    this->b = b;
    this->a = a;
  }
  float r;
  float g;
  float b;
  float a;
};
struct ColorU8
{
public:
  ColorU8(u8 r, u8 g, u8 b, u8 a)
  {
    this->r = r;
    this->g = g;
    this->b = b;
    this->a = a;
  }
  u8 r;
  u8 g;
  u8 b;
  u8 a;
};

class ConvexHull2D
{
public:
  Point2*             points;
  int                 count;
  int                 cap;
  static ConvexHull2D create_andrew(Point2* points, int point_count);
  static ConvexHull2D create_quick(Point2* points, int point_count);
};

Vector3 interpolate_translation(Vector3 v0, Vector3 v1, f32 t);
Mat44   interpolate_transforms(Mat44 first, Mat44 second, f32 time);
#define BLACK Color(0, 0, 0, 1)
#define WHITE Color(1, 1, 1, 1)
#define RED   Color(1, 0, 0, 1)
#define GREEN Color(0, 1, 0, 1)
#define BLUE  Color(0, 0, 1, 1)

#endif
