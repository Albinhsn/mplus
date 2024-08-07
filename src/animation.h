#ifndef ANIMATION_H
#define ANIMATION_H

#include "shader.h"
#include "files.h"
#include "vector.h"

struct Joint
{
public:
  Mat44 m_invBindPose;
  char* m_name;
  u32   m_name_length;
  i8    m_iParent;
};

struct Skeleton
{
  u32    joint_count;
  Joint* joints;
};

struct JointPose
{
public:
  Mat44* local_transforms;
  f32*   steps;
  u32    step_count;
};

struct SkinnedVertex
{
  Vector3 position;
  Vector2 uv;
  Vector3 normal;
  float   joint_weight[4];
  u32     joint_index[4];
  Vector3 tangent;
  Vector3 bitangent;
  void    debug()
  {
    position.debug();
    uv.debug();
    normal.debug();
    printf("%d %d %d %d\n", joint_index[0], joint_index[1], joint_index[2], joint_index[3]);
    printf("%f %f %f %f\n", joint_weight[0], joint_weight[1], joint_weight[2], joint_weight[3]);
  }
};

struct Animation
{
  char*      name;
  f32        duration;
  f32        scaling;
  JointPose* poses;
  u32        joint_count;
};

struct AnimationModel
{
public:
  SkinnedVertex* vertices;
  u32*           indices;
  Animation*     animations;
  u32            animation_count;
  Skeleton       skeleton;
  u64            vertex_count;
  u64            index_count;
  void           debug();
};

void calculate_new_pose(Mat44* poses, u32 count, Animation* animation, u32 ticks);
void update_animation(Skeleton* skeleton, Animation* animation, Mat44* transforms, u32 ticks);
bool parse_animation_file(AnimationModel* model, const char* filename, const char* mapping_location);

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
  i32            animation_controller_index;
  AnimationData* animation_data;
  Vector3*       vertices;
  u32            vertex_data_size;
  u32            index_count;
  u32            vertex_count;
};
// essentially be able to queue/start an animation at some point in time
// what queue/start really is is just change the current one
//  the current one will have a next otherwise it's looping
//
struct AnimationController
{
  AnimationData* animation_data;
  i32            current_animation;
  u32            current_animation_start_tick;
  i32            next_animation_index;
  Mat44*         transforms;
};

#endif
