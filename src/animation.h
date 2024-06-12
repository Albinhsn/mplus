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
public:
  XML_Node*  find_key_with_tag(const char* key_name, const char* tag_name, const char* tag_value);
  XML_Node*  find_key(const char* key_name);
  XML_Tag*   find_tag(const char* tag_name);
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


struct Joint
{
public:
  void  debug();
  Mat44 m_invBindPose;
  Mat44 m_mat;
  char* m_name;
  u32   m_name_length;
  u8    m_iParent;
};

struct Skeleton
{
  u32    joint_count;
  Joint* joints;
};

struct String
{
  char* s;
  u32   l;
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
void debug_xml_node(XML_Node* xml);
bool remove_xml_key(XML_Node* xml, const char* node_name);
void debug_xml(XML* xml);
void write_xml_to_file(XML* xml, const char* filename);

#endif
