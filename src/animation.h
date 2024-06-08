#ifndef ANIMATION_H
#define ANIMATION_H

#include "vector.h"

enum XML_TYPE
{
  XML_PARENT,
  XML_CONTENT
};

struct XML_Tag
{
  const char* name;
  u64         name_length;
  const char* value;
  u64         value_length;
};
struct XML_Header
{
  const char* name;
  u64         name_length;
  XML_Tag*    tags;
  u64         tag_count;
  u64         tag_capacity;
};

struct XML
{
  XML*       next;
  XML*       parent;
  XML_TYPE   type;
  XML_Header header;
  union
  {
    XML* child;
    struct
    {
      const char* content;
      u64         content_length;
    };
  };
};

struct Quaternion
{
  f32 x, y, z, w;
};

// struct Joint {
//   Mat43 m_invBindPose;
//   const char *m_name;
//   u8 m_iParent;
// };

// A skeleton contains a hierarchy of joints
// Bone/Joint is the thing we move that vertex positions are tied to

struct Joint
{
  f32 position[3];
  f32 normal[3];
  f32 u, v;
  u8  jointIndex[4];
  f32 jointWeight[3];
};

struct Skeleton
{
  u32    m_jointCount;
  Joint* m_aJoint;
};

struct JointPose
{
  Quaternion m_rot;
  Vector3    m_trans;
  f32        m_scale;
};

struct SkeletonPose
{
  Skeleton*  m_pSkeleton;
  JointPose* m_aLocalPose;
  Mat44*     m_aGlobalPose;
};

struct AnimationSample
{
  JointPose* m_aJointPose;
};

struct AnimationClip
{
  Skeleton*        m_pSkeleton;
  f32              m_framesPerSecond;
  u32              m_frameCount;
  AnimationSample* m_aSamples;
  bool             m_isLooping;
};

struct SkinnedVertex
{
  float position[3];
  float normal[3];
  float u, v;
  u8    jointIndex[4];
  float jointWeight[3];
};

bool sta_parse_collada_file(XML* xml, const char* filename);
void debug_xml(XML* xml);

#endif
