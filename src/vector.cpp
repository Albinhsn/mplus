#include "vector.h"
#include "common.h"
#include <cmath>
#include <cstring>

float Mat22::determinant()
{
  return this->rc[0][0] * this->rc[1][1] - this->rc[0][1] * this->rc[1][0];
}

Mat44 Mat44::inverse()
{
  Mat33 m00 = {
      .rc = {{this->rc[1][1], this->rc[1][2], this->rc[1][3]}, //
 {this->rc[2][1], this->rc[2][2], this->rc[2][3]}, //
 {this->rc[3][1], this->rc[3][2], this->rc[3][3]}}  //
  };
  Mat33 m01 = {
      .rc = {{this->rc[1][0], this->rc[1][2], this->rc[1][3]}, //
 {this->rc[2][0], this->rc[2][2], this->rc[2][3]}, //
 {this->rc[3][0], this->rc[3][2], this->rc[3][3]}}  //
  };
  Mat33 m02 = {
      .rc = {{this->rc[1][0], this->rc[1][1], this->rc[1][3]}, //
 {this->rc[2][0], this->rc[2][1], this->rc[2][3]}, //
 {this->rc[3][0], this->rc[3][1], this->rc[3][3]}}
  }; //
  Mat33 m03 = {
      .rc = {{this->rc[1][0], this->rc[1][1], this->rc[1][2]}, //
 {this->rc[2][0], this->rc[2][1], this->rc[2][2]}, //
 {this->rc[3][0], this->rc[3][1], this->rc[3][2]}}  //
  };

  Mat33 m10 = {
      .rc = {{this->rc[0][1], this->rc[0][2], this->rc[0][3]}, //
 {this->rc[2][1], this->rc[2][2], this->rc[2][3]}, //
 {this->rc[3][1], this->rc[3][2], this->rc[3][3]}}  //
  };
  Mat33 m11 = {
      .rc = {{this->rc[0][0], this->rc[0][2], this->rc[0][3]}, //
 {this->rc[2][0], this->rc[2][2], this->rc[2][3]}, //
 {this->rc[3][0], this->rc[3][2], this->rc[3][3]}}  //
  };
  Mat33 m12 = {
      .rc = {{this->rc[0][0], this->rc[0][1], this->rc[0][3]}, //
 {this->rc[2][0], this->rc[2][1], this->rc[2][3]}, //
 {this->rc[3][0], this->rc[3][1], this->rc[3][3]}}  //
  };
  Mat33 m13 = {
      .rc = {{this->rc[0][0], this->rc[0][1], this->rc[0][2]}, //
 {this->rc[2][0], this->rc[2][1], this->rc[2][2]}, //
 {this->rc[3][0], this->rc[3][1], this->rc[3][2]}}  //
  };

  Mat33 m20 = {
      .rc = {{this->rc[0][1], this->rc[0][2], this->rc[0][3]}, //
 {this->rc[1][1], this->rc[1][2], this->rc[1][3]}, //
 {this->rc[3][1], this->rc[3][2], this->rc[3][3]}}  //
  };

  Mat33 m21 = {
      .rc = {{this->rc[0][0], this->rc[0][2], this->rc[0][3]}, //
 {this->rc[1][0], this->rc[1][2], this->rc[1][3]}, //
 {this->rc[3][0], this->rc[3][2], this->rc[3][3]}}  //
  };
  Mat33 m22 = {
      .rc = {{this->rc[0][0], this->rc[0][1], this->rc[0][3]}, //
 {this->rc[1][0], this->rc[1][1], this->rc[1][3]}, //
 {this->rc[3][0], this->rc[3][1], this->rc[3][3]}}  //
  };
  Mat33 m23 = {
      .rc = {{this->rc[0][0], this->rc[0][1], this->rc[0][2]}, //
 {this->rc[1][0], this->rc[1][1], this->rc[1][2]}, //
 {this->rc[3][0], this->rc[3][1], this->rc[3][2]}}  //
  };
  Mat33 m30 = {
      .rc = {{this->rc[0][1], this->rc[0][2], this->rc[0][3]}, //
 {this->rc[1][1], this->rc[1][2], this->rc[1][3]}, //
 {this->rc[2][1], this->rc[2][2], this->rc[2][3]}}  //
  };
  Mat33 m31 = {
      .rc = {{this->rc[0][0], this->rc[0][2], this->rc[0][3]}, //
 {this->rc[1][0], this->rc[1][2], this->rc[1][3]}, //
 {this->rc[2][0], this->rc[2][2], this->rc[2][3]}}  //
  };
  Mat33 m32 = {
      .rc = {{this->rc[0][0], this->rc[0][1], this->rc[0][3]}, //
 {this->rc[1][0], this->rc[1][1], this->rc[1][3]}, //
 {this->rc[2][0], this->rc[2][1], this->rc[2][3]}}  //
  };
  Mat33 m33 = {
      .rc = {{this->rc[0][0], this->rc[0][1], this->rc[0][2]}, //
 {this->rc[1][0], this->rc[1][1], this->rc[1][2]}, //
 {this->rc[2][0], this->rc[2][1], this->rc[2][2]}}  //
  };
  Mat44 res = {
      .rc = {{m00.determinant(), -m01.determinant(), m02.determinant(), -m03.determinant()},
             {-m10.determinant(), m11.determinant(), -m12.determinant(), m13.determinant()},
             {m20.determinant(), -m21.determinant(), m22.determinant(), -m23.determinant()},
             {-m30.determinant(), m31.determinant(), -m32.determinant(), m33.determinant()}}
  };
  // debugmat4x4(res);

  f32 det   = this->determinant();
  f32 scale = 1.0f / det;
  res.scale(Vector3(scale, scale, scale));
  return res;
}

Mat44 Mat44::rotate_x(f32 degrees)
{
  float r = DEGREES_TO_RADIANS(degrees);
  Mat44 m = {};
  m.identity();
  m.rc[1][1] = cosf(r);
  m.rc[1][2] = -sinf(r);
  m.rc[2][1] = sinf(r);
  m.rc[2][2] = cosf(r);
  return this->mul(m);
}

Mat44 Mat44::scale(Vector3 v)
{
  Mat44 m    = {};
  m.rc[0][0] = v.x;
  m.rc[1][1] = v.y;
  m.rc[2][2] = v.z;
  m.rc[3][3] = 1.0f;

  return this->mul(m);
}
Mat44 Mat44::rotate_y(f32 degrees)
{
  float r = DEGREES_TO_RADIANS(degrees);
  Mat44 m = {};
  m.identity();
  m.rc[0][0] = cosf(r);
  m.rc[0][2] = -sinf(r);
  m.rc[2][0] = sinf(r);
  m.rc[2][2] = cosf(r);
  return this->mul(m);
}

void Mat44::debug()
{
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      printf("%f", this->rc[i][j]);
      if (j < 3)
      {
        printf(", ");
      }
      else
      {
      }
    }
    printf("\n");
  }
}
void Mat44::perspective(f32 fov, f32 screen_aspect, f32 screen_near, f32 screen_depth)
{
  fov            = DEGREES_TO_RADIANS(fov);
  float c        = 1.0f / (tan(fov * 0.5f));
  float a        = screen_aspect;

  float f        = screen_depth;
  float n        = screen_near;

  this->rc[0][0] = c / a;
  this->rc[0][1] = 0.0f;
  this->rc[0][2] = 0.0f;
  this->rc[0][3] = 0.0f;

  this->rc[1][0] = 0.0f;
  this->rc[1][1] = c;
  this->rc[1][2] = 0.0f;
  this->rc[1][3] = 0.0f;

  this->rc[2][0] = 0.0f;
  this->rc[2][1] = 0.0f;
  this->rc[2][2] = -((f + n) / (f - n));
  this->rc[2][3] = -(2.0f * ((f * n) / (f - n)));

  this->rc[3][0] = 0.0f;
  this->rc[3][1] = 0.0f;
  this->rc[3][2] = -1.0f;
  this->rc[3][3] = 0.0f;
}

Mat44 Mat44::mul(Mat44 m)
{
  Mat44 res = {};
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      for (int k = 0; k < 4; k++)
      {
        res.rc[i][j] += this->rc[k][j] * m.rc[i][k];
      }
    }
  }
  return res;
}
Mat44 Mat44::rotate_z(f32 degrees)
{

  float r    = degrees * PI / 180.0f;
  Mat44 m    = {};

  m.rc[0][0] = cosf(r);
  m.rc[0][1] = -sinf(r);
  m.rc[1][0] = sinf(r);
  m.rc[1][1] = cosf(r);
  m.rc[2][2] = 1;
  m.rc[3][3] = 1;

  return this->mul(m);
}
Vector4 Mat44::mul(Vector4 v)
{
  Vector4 res(0, 0, 0, 0);
  for (int i = 0; i < 4; i++)
  {
    res.x += this->rc[i][0] * v.x;
    res.y += this->rc[i][1] * v.y;
    res.z += this->rc[i][2] * v.z;
    res.w += this->rc[i][3] * v.w;
  }

  return res;
}

Mat44 Mat44::transpose()
{
  Mat44 out = {};
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      out.rc[i][j] = this->rc[j][i];
    }
  }
  return out;
}

Mat44 Mat44::look_at(Vector3 x, Vector3 y, Vector3 z)
{
  Mat44 out    = {};
  out.rc[0][0] = x.x;
  out.rc[0][1] = x.y;
  out.rc[0][2] = x.z;
  out.rc[0][3] = 0.0f;

  out.rc[1][0] = y.x;
  out.rc[1][1] = y.y;
  out.rc[1][2] = y.z;
  out.rc[1][3] = 0.0f;

  out.rc[2][0] = z.x;
  out.rc[2][1] = z.y;
  out.rc[2][2] = z.z;
  out.rc[2][3] = 0.0f;

  out.rc[0][3] = 0.0f;
  out.rc[1][3] = 0.0f;
  out.rc[2][3] = 0.0f;
  out.rc[3][3] = 1.0f;

  return out;
}

Mat44 Mat44::translate(Vector3 v)
{
  Mat44 m = {};
  m.identity();

  m.rc[0][3] = v.x;
  m.rc[1][3] = v.y;
  m.rc[2][3] = v.z;

  return this->mul(m);
}

void Mat44::identity()
{
  memset(&this->m[0], 0, 16 * sizeof(float));
  this->rc[0][0] = 1.0f;
  this->rc[1][1] = 1.0f;
  this->rc[2][2] = 1.0f;
  this->rc[3][3] = 1.0f;
}

float Mat44::determinant()
{
  Mat33 m00 = {
      .rc = {{this->rc[1][1], this->rc[1][2], this->rc[1][3]}, {this->rc[2][1], this->rc[2][2], this->rc[2][3]}, {this->rc[3][1], this->rc[3][2], this->rc[3][3]}},
  };
  Mat33 m01 = {
      .rc = {{this->rc[1][0], this->rc[1][2], this->rc[1][3]}, {this->rc[2][0], this->rc[2][2], this->rc[2][3]}, {this->rc[3][0], this->rc[3][2], this->rc[3][3]}},
  };
  Mat33 m02 = {
      .rc = {{this->rc[1][0], this->rc[1][1], this->rc[1][3]}, {this->rc[2][0], this->rc[2][1], this->rc[2][3]}, {this->rc[3][0], this->rc[3][1], this->rc[3][3]}},
  };
  Mat33 m03 = {
      .rc = {{this->rc[1][0], this->rc[1][1], this->rc[1][2]}, //
 {this->rc[2][0], this->rc[2][1], this->rc[2][2]}, //
 {this->rc[3][0], this->rc[3][1], this->rc[3][2]}}, //
  };

  f32 d0 = this->rc[0][0] * m00.determinant();
  f32 d1 = this->rc[0][1] * m01.determinant();
  f32 d2 = this->rc[0][2] * m02.determinant();
  f32 d3 = this->rc[0][3] * m03.determinant();
  return (d0 - d1 + d2 - d3);
}

float Mat33::determinant()
{
  Vector3 u = {this->rc[0][0], this->rc[0][1], this->rc[0][2]};
  Vector3 v = {this->rc[1][0], this->rc[1][1], this->rc[1][2]};
  Vector3 w = {this->rc[2][0], this->rc[2][1], this->rc[2][2]};
  return u.dot(v.cross(w));
}

float Vector3::len()
{
  return sqrt(this->x * this->x + this->y * this->y + this->z * this->z);
}

Vector3 Vector3::sub(Vector3 v)
{
  return {this->x - v.x, this->y - v.y, this->z - v.z};
}
Vector3 Vector3::cross(Vector3 v0, Vector3 v1)
{
  Vector3 out = {
      v0.y * v1.z - v0.z * v1.y,    //
      -(v0.x * v1.z - v0.z * v1.x), //
      v0.x * v1.y - v0.y * v1.x     //
  };
  return out;
}

float Vector2::dot_perp(Vector2 v)
{
  return this->x * v.y - this->y * v.x;
}

float Vector2::dot(Vector2 v)
{
  return this->x * v.x + this->y * v.y;
}

Vector3 Vector3::cross(Vector3 v)
{
  Vector3 out = {
      this->y * v.z - this->z * v.y,    //
      -(this->x * v.z - this->z * v.x), //
      this->x * v.y - this->y * v.x     //
  };
  return out;
}
float Vector3::dot(Vector3 v)
{
  return this->x * v.x + this->y * v.y + this->z * v.z;
}

void barycentric(Point3 a, Point3 b, Point3 c, Point3 p, float& u, float& v, float& w)
{
  Vector3 v0 = b.sub(a), v1 = c.sub(a), v2 = p.sub(a);
  float   d00   = v0.dot(v0);
  float   d01   = v0.dot(v1);
  float   d11   = v1.dot(v1);
  float   d20   = v2.dot(v1);
  float   d21   = v2.dot(v2);

  float   denom = d00 * d11 - d01 * d01;

  v             = (d11 * d20 - d01 * d21) / denom;
  w             = (d00 * d21 - d01 * d20) / denom;
  u             = 1.0f - v - w;
}
