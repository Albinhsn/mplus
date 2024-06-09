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

struct Joint
{
  Mat44 m_invBindPose;
  char* m_name;
  u8    m_iParent;
};

struct Skeleton
{
  u32    joint_count;
  Joint* joints;
};

// just an affine transformation
struct JointPose
{
  float* t;
  Mat44* local_transform;
  u32 steps;
};

struct Animation
{
  JointPose* poses;
};

struct SkinnedVertex
{
  Vector3 position;
  Vector2 uv;
  Vector3 normal;
  u8      joint_index[4];
  float   joint_weight[4];
};

struct AnimationModel
{
  SkinnedVertex* vertices;
  u32*           indices;
  Animation      animations;
  Skeleton       skeleton;
  u64            vertex_count;
  u8             clip_count;
};

bool      sta_collada_parse_from_file(AnimationModel* animation, const char* filename);
void      debug_xml_node(XML_Node* xml);
XML_Node* sta_xml_find_key(XML_Node* xml, const char* node_name);
bool      remove_xml_key(XML_Node* xml, const char* node_name);
void      debug_xml(XML* xml);
void      write_xml_to_file(XML* xml, const char* filename);

#endif
