#ifndef ANIMATION_H
#define ANIMATION_H

#include "files.h"
#include "vector.h"

enum XML_TYPE
{
  XML_PARENT,
  XML_CONTENT,
  XML_CLOSED
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

enum XML_Encoding
{
  UTF_8,
  UNKNOWN
};
struct XML_Node
{
  XML_Node*  next;
  XML_Node*  parent;
  XML_TYPE   type;
  XML_Header header;
  union
  {
    XML_Node* child;
    struct
    {
      const char* content;
      u64         content_length;
    };
  };
};

struct XML
{
  XML_Node head;
  XML_Tag* version_and_encoding;
  u64      version_and_encoding_length;
  u64      version_and_encoding_capacity;
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

struct AnimationModel
{
  ModelData model_data;
};

bool      sta_collada_parse_from_file(AnimationModel* animation, const char* filename);
void      debug_xml_node(XML_Node* xml);
XML_Node* sta_xml_find_key(XML_Node* xml, const char* node_name);
bool      remove_xml_key(XML_Node* xml, const char* node_name);
void      debug_xml(XML* xml);
void      write_xml_to_file(XML* xml, const char* filename);

#endif
