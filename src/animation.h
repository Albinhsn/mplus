#ifndef ANIMATION_H
#define ANIMATION_H

#include "vector.h"
struct Quaternion {};

// struct Joint {
//   Mat43 m_invBindPose;
//   const char *m_name;
//   u8 m_iParent;
// };

struct Joint {
  f32 position[3];
  f32 normal[3];
  f32 u, v;
  u8 jointIndex[4];
  f32 jointWeight[3];
};

struct Skeleton {
  u32 m_jointCount;
  Joint *m_aJoint;
};

struct JointPose {
  Quaternion m_rot;
  Vector3 m_trans;
  f32 m_scale;
};

struct SkeletonPose {
  Skeleton *m_pSkeleton;
  JointPose *m_aLocalPose;
  Mat44 *m_aGlobalPose;
};

struct AnimationSample {
  JointPose *m_aJointPose;
};

struct AnimationClip {
  Skeleton *m_pSkeleton;
  f32 m_framesPerSecond;
  u32 m_frameCount;
  AnimationSample *m_aSamples;
  bool m_isLooping;
};

struct SkinnedVertex {
  float position[3];
  float normal[3];
  float u, v;
  u8 jointIndex[4];
  float jointWeight[3];
};

#endif
