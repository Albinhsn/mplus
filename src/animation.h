#ifndef ANIMATION_H
#define ANIMATION_H

#include "files.h"
#include "vector.h"

struct Joint
{
public:
  void  debug();
  Mat44 m_invBindPose;
  Mat44 m_mat;
  String name;
  char* m_name;
  u32   m_name_length;
  u8    m_iParent;
};

struct Skeleton
{
  u32    joint_count;
  Joint* joints;
};


struct JointPose
{
public:
  Mat44* local_transform;
};

struct Animation
{
  f32        duration;
  JointPose* poses;
  f32*       steps;
  u32        pose_count;
  u32        joint_count;
};

struct SkinnedVertex
{
public:
  void    debug();
  Vector3 position;
  Vector2 uv;
  Vector3 normal;
  float   joint_weight[4];
  u32     joint_index[4];
};

struct AnimationModel
{
public:
  SkinnedVertex* vertices;
  u32*           indices;
  Animation      animations;
  Skeleton       skeleton;
  u64            vertex_count;
  u8             clip_count;
  void           debug();
};
struct ColladaFaceIndexOrder
{
  int position_index;
  int normal_index;
  int uv_index;
};
struct ColladaGeometry
{
  Vector3*               vertices;
  Vector2*               uv;
  Vector3*               normals;
  ColladaFaceIndexOrder* faces;
  int                    face_count;
};

struct ColladaJointAndWeightData
{
  int joint_index[4];
  f32 weight[4];
  int count;
};
struct ColladaControllers
{
  int                        vertex_weights_count; // vertex_count?
  ColladaJointAndWeightData* joint_and_weight_idx;
  Joint*                     joints;
  u32                        joint_count;
};

bool sta_collada_parse_from_file(AnimationModel* animation, const char* filename);

#endif
