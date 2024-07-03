#ifndef ANIMATION_H
#define ANIMATION_H

#include "shader.h"
#include "files.h"
#include "vector.h"

struct Joint
{
public:
  Mat44  m_invBindPose;
  Mat44  m_mat;
  String name;
  char*  m_name;
  u32    m_name_length;
  u8     m_iParent;
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
  f32*   steps;
  u32    step_count;
};

struct Animation
{
  f32        duration;
  JointPose* poses;
  u32        joint_count;
};

struct SkinnedVertex
{
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
  u64            index_count;
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
void calculate_new_pose(Mat44* poses, u32 count, Animation* animation, u32 ticks);
void update_animation(Skeleton* skeleton, Animation* animation, Shader shader, u32 ticks);

struct JointData
{
  int        index;
  char*      name;
  u32        name_length;
  Mat44      local_bind_transform;
  Mat44      inverse_bind_transform;
  JointData* children;
  u32        children_cap;
  u32        children_count;
};

struct SkeletonData
{
  int        joint_count;
  JointData* head_joint;
};

struct JointWeight
{
  u32 joint_id;
  f32 weight;
};

struct VertexSkinData
{
  JointWeight* data;
  u32          count;
};

struct SkinningData
{
  char**          joint_order;
  u32             joint_order_count;
  VertexSkinData* vertex_skin_data;
  u32             vertex_skin_data_count;
};

struct MeshData
{
  SkinnedVertex* vertices;
  u32*           indices;
  u32            vertex_count;
};

struct JointTransformData
{
  char* name;
  u32   length;
  Mat44 local_transform;
};
struct KeyFrameData
{
  float               time;
  JointTransformData* transforms;
};

struct ColladaAnimationData
{
  KeyFrameData* key_frames;
  float*        timesteps;
  float         duration;
  u32           count;
};

struct ColladaModelData
{
  SkeletonData         skeleton_data;
  MeshData             mesh_data;
  SkinningData         skinning_data;
  ColladaAnimationData animation_data;
};

enum ModelType
{
  MODEL_TYPE_ANIMATION_MODEL,
  MODEL_TYPE_MODEL_DATA,
};

struct AnimationData
{
  Animation* animations;
  u32        animation_count;
  Skeleton   skeleton;
};

struct Model
{
  const char*    name;
  void*          vertex_data;
  u32*           indices;
  AnimationData* animation_data;
  u32            vertex_data_size;
  u32            index_count;
  u32            vertex_count;
};

#endif
