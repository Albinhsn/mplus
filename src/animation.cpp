#include "animation.h"
#include "common.h"
#include "files.h"
#include "platform.h"
#include "vector.h"
#include <cassert>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void AnimationModel::debug()
{

  printf("Joints: %d\n", this->skeleton.joint_count);
  printf("Vertex count: %ld\nIndex count: %ld\n", this->vertex_count, this->index_count);
  for (u32 i = 0; i < this->vertex_count; i++)
  {
    this->vertices[i].debug();
  }
  printf("Animations: %d\n", this->animation_count);
  for (u32 i = 0; i < this->animation_count; i++)
  {
    printf("Animation: '%s' duration: %f\n", this->animations[i].name, this->animations[i].duration);
  }
}

void calculate_new_pose(Mat44* poses, u32 count, Animation* animation, u32 ticks)
{

  const f32 EPSILON   = 0.0001f;
  u64       loop_time = (u64)(1000 * animation->duration * animation->scaling);
  f32       time      = (ticks % loop_time) / (f32)loop_time;
  if (time == 0)
  {
    time += EPSILON;
  }

  for (u32 i = 0; i < animation->joint_count; i++)
  {
    if (time >= animation->duration - EPSILON)
    {
      time = animation->duration;
    }
    u32        pose_idx = 0;
    JointPose* pose     = &animation->poses[i];
    if (pose->step_count == 0)
    {
      poses[i].identity();
      continue;
    }
    for (; pose_idx < pose->step_count && pose->steps[pose_idx] < time; pose_idx++)
      ;

    pose_idx                 = MAX(1, pose_idx);
    float time_between_poses = (time - pose->steps[pose_idx - 1]) / (pose->steps[pose_idx] - pose->steps[pose_idx - 1]);

    poses[i]                 = interpolate_transforms(pose->local_transforms[pose_idx - 1], pose->local_transforms[pose_idx], time_between_poses);
  }
}

void update_animation(Skeleton* skeleton, Animation* animation, Mat44* transforms, u32 ticks)
{
  Mat44 current_poses[skeleton->joint_count];
  calculate_new_pose(current_poses, skeleton->joint_count, animation, ticks);

  Mat44 parent_transforms[skeleton->joint_count];
  for (u32 i = 0; i < skeleton->joint_count; i++)
  {
    // current_poses[i].debug();
    parent_transforms[i].identity();
  }

  for (u32 i = 0; i < skeleton->joint_count; i++)
  {
    Joint* joint            = &skeleton->joints[i];

    Mat44  current_local    = current_poses[i];
    Mat44  parent_transform = {};
    parent_transform.identity();
    if (joint->m_iParent != -1)
    {
      parent_transform = parent_transforms[joint->m_iParent];
    }

    Mat44 current_transform = current_local.mul(parent_transform);
    parent_transforms[i]    = current_transform;
    transforms[i]           = joint->m_invBindPose.mul(current_transform);
  }
}

static u8 get_joint_index_from_id(char** names, u32 count, char* name, u64 length)
{
  for (unsigned int i = 0; i < count; i++)
  {
    if (strlen(names[i]) == length && strncmp(name, names[i], length) == 0)
    {
      return i;
    }
  }
  assert(0 && "how could this happen to me");
}
static u8 get_joint_index_from_id(Skeleton* skeleton, char* name, u64 length)
{
  for (unsigned int i = 0; i < skeleton->joint_count; i++)
  {
    if (skeleton->joints[i].m_name_length == length && strncmp(name, skeleton->joints[i].m_name, length) == 0)
    {
      return i;
    }
  }
  assert(0 && "Couldn't find joint by this name?");
}

bool parse_animation_file(AnimationModel* model, const char* filename, const char* animation_mapping_location)
{

  assert(strlen(filename) > 4 && compare_strings("anim", &filename[strlen(filename) - 4]) && "Expected .anim file!");

  Buffer buffer = {};

  if (!sta_read_file(&buffer, filename))
  {
    return false;
  }

  u32 node_count = buffer.parse_int();
  buffer.skip_whitespace();

  // number of nodes
  // name idx

  Skeleton* skeleton    = &model->skeleton;
  skeleton->joints      = sta_allocate_struct(Joint, node_count);
  skeleton->joint_count = node_count;
  // node hierarchy
  // inv bind
  for (u32 i = 0; i < node_count; i++)
  {
    Joint* joint  = &skeleton->joints[i];
    joint->m_name = buffer.parse_string();
    buffer.skip_whitespace();
    joint->m_iParent = buffer.parse_int();
    buffer.skip_whitespace();
    for (u32 j = 0; j < 16; j++)
    {
      joint->m_invBindPose.m[j] = buffer.parse_float();
      buffer.skip_whitespace();
    }
  }

  // skinned vertex count
  // skinned vertex data
  model->vertex_count = buffer.parse_int();
  model->vertices     = sta_allocate_struct(SkinnedVertex, model->vertex_count);
  buffer.skip_whitespace();
  for (u32 i = 0; i < model->vertex_count; i++)
  {

    SkinnedVertex* vertex = &model->vertices[i];
    // position
    vertex->position.x = buffer.parse_float();
    buffer.skip_whitespace();
    vertex->position.y = buffer.parse_float();
    buffer.skip_whitespace();
    vertex->position.z = buffer.parse_float();
    buffer.skip_whitespace();

    // uv
    vertex->uv.x = buffer.parse_float();
    buffer.skip_whitespace();
    vertex->uv.y = -buffer.parse_float();
    buffer.skip_whitespace();

    // normal
    vertex->normal.x = buffer.parse_float();
    buffer.skip_whitespace();
    vertex->normal.y = buffer.parse_float();
    buffer.skip_whitespace();
    vertex->normal.z = buffer.parse_float();
    buffer.skip_whitespace();

    // joint/weight pair count
    memset(vertex->joint_index, 0, ArrayCount(vertex->joint_index) * sizeof(vertex->joint_index[0]));
    memset(vertex->joint_weight, 0, ArrayCount(vertex->joint_weight) * sizeof(vertex->joint_weight[0]));
    u32 jw_count = buffer.parse_int();
    buffer.skip_whitespace();

    // joint indices
    for (u32 i = 0; i < jw_count; i++)
    {
      vertex->joint_index[i] = buffer.parse_int();
      buffer.skip_whitespace();
    }

    // weights
    for (u32 i = 0; i < jw_count; i++)
    {
      vertex->joint_weight[i] = buffer.parse_float();
      buffer.skip_whitespace();
    }
  }

  // index count
  model->index_count = buffer.parse_int();
  model->indices     = sta_allocate_struct(u32, model->index_count);
  buffer.skip_whitespace();
  // index data
  for (u32 i = 0; i < model->index_count; i++)
  {
    model->indices[i] = buffer.parse_int();
    buffer.skip_whitespace();
  }

  // animation count
  //  name
  //  duration
  //  step count
  //  node name
  //  steps
  //  matrices
  model->animation_count = buffer.parse_int();
  model->animations      = sta_allocate_struct(Animation, model->animation_count);
  buffer.skip_whitespace();
  for (u32 i = 0; i < model->animation_count; i++)
  {
    Animation* animation = &model->animations[i];

    animation->name      = buffer.parse_string();
    printf("Found animation '%s'\n", animation->name);
    buffer.skip_whitespace();
    animation->duration = buffer.parse_float();
    buffer.skip_whitespace();
    animation->joint_count = buffer.parse_int();
    assert(animation->joint_count == skeleton->joint_count && "Mismatch in animation pose count and skeleton joint count, please implement fix :)");
    animation->poses = sta_allocate_struct(JointPose, animation->joint_count);
    buffer.skip_whitespace();
    for (u32 j = 0; j < animation->joint_count; j++)
    {
      JointPose* pose      = &animation->poses[j];
      char*      node_name = buffer.parse_string();
      buffer.skip_whitespace();

      pose->step_count = buffer.parse_int();
      buffer.skip_whitespace();
      pose->steps            = sta_allocate_struct(f32, pose->step_count);
      pose->local_transforms = sta_allocate_struct(Mat44, pose->step_count);

      for (u32 k = 0; k < pose->step_count; k++)
      {
        pose->steps[k] = buffer.parse_float();
        buffer.skip_whitespace();
      }

      for (u32 k = 0; k < pose->step_count; k++)
      {
        Mat44* lt = &pose->local_transforms[k];
        for (u32 matrix_idx = 0; matrix_idx < 16; matrix_idx++)
        {
          lt->m[matrix_idx] = buffer.parse_float();
          buffer.skip_whitespace();
        }
      }
    }
  }

  if (animation_mapping_location)
  {
    buffer       = {};
    buffer.index = 0;

    if (!sta_read_file(&buffer, animation_mapping_location))
    {
      return false;
    }

    while (!buffer.is_out_of_bounds())
    {
      char* mapped_name = buffer.parse_string();
      buffer.skip_whitespace();
      char* original_name = buffer.parse_string();
      buffer.skip_whitespace();
      f32  scaling = buffer.parse_float();
      bool found   = false;
      for (u32 i = 0; i < model->animation_count; i++)
      {
        if (compare_strings(model->animations[i].name, original_name))
        {
          printf("Swapped %s for %s\n", original_name, mapped_name);
          model->animations[i].name    = mapped_name;
          model->animations[i].scaling = scaling;
          found                        = true;
          break;
        }
      }
      if (!found)
      {
        printf("Coulnd't swap '%s'\n", original_name);
      }
      buffer.skip_whitespace();
    }
  }
  else
  {
    logger.warning("Found no mapping for '%s'!\n", filename);
  }

  return true;
}
