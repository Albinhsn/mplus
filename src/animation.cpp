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

void SkinnedVertex::debug()
{
  printf("SkinnedVertex: \n");
  printf("Position: ");
  this->position.debug();
  printf("Normal: ");
  this->normal.debug();
  printf("Uv: ");
  this->uv.debug();
  printf("Joint indices: %d %d %d %d\n", this->joint_index[0], this->joint_index[1], this->joint_index[2], this->joint_index[3]);
  printf("Joint weights: %f %f %f %f\n", this->joint_weight[0], this->joint_weight[1], this->joint_weight[2], this->joint_weight[3]);
}

void Joint::debug()
{
  printf("Joint %s:\n", this->m_name);
  printf("Parent: %d\n", this->m_iParent);
  this->m_invBindPose.debug();
}

void AnimationModel::debug()
{
  printf("Skeletons:\n");
  for (u32 i = 0; i < this->skeleton.joint_count; i++)
  {
    this->skeleton.joints[i].debug();
  }

  printf("SkinnedVertices:\n");
  for (u32 i = 0; i < this->vertex_count; i++)
  {
    this->vertices[i].debug();
  }
}

static Buffer sta_xml_create_buffer_from_content(XML_Node* node)
{
  Buffer buffer = {};
  buffer.buffer = (char*)node->content;
  buffer.index  = 0;
  buffer.len    = node->content_length;
  return buffer;
}

int parse_int_from_string(const char* str)
{
  Buffer buffer = {};
  buffer.len    = strlen(str);
  buffer.buffer = (char*)str;
  return parse_int_from_string(&buffer);
}

void free_xml_node(XML_Node* node)
{
  if (node->type == XML_PARENT)
  {
    XML_Node* child = node->child;
    while (child != 0)
    {
      XML_Node* next = child->next;
      free_xml_node(child);
      child = next;
    }
  }

  sta_deallocate(node->header.tags, sizeof(XML_Tag) * node->header.tag_capacity);
  sta_deallocate(node, sizeof(XML_Node));
}

bool remove_xml_key(XML_Node* xml, const char* node_name, u64 node_name_length)
{

  XML_Node* prev = xml->child;
  XML_Node* curr = xml->child;
  while (curr != 0)
  {
    if (curr->header.name_length == node_name_length && strncmp(curr->header.name, node_name, node_name_length) == 0)
    {
      if (curr == xml->child)
      {
        xml->child = curr->next;
        if (xml->child == 0)
        {
          xml->type = XML_CLOSED;
        }
      }
      else
      {

        prev->next = curr->next;
      }
      free_xml_node(curr);
      return true;
    }
    prev = curr;
    curr = curr->next;
  }
  return false;
}
XML_Node* XML_Node::find_key_with_tag(const char* key_name, const char* tag_name, const char* tag_value)
{
  u64       node_name_length = strlen(key_name);
  XML_Node* curr             = this->child;
  while (curr != 0)
  {
    if (curr->header.name_length == node_name_length && strncmp(curr->header.name, key_name, node_name_length) == 0)
    {
      XML_Tag* tag = curr->find_tag(tag_name);
      if (tag->value_length == strlen(tag_value) && strncmp(tag->value, tag_value, tag->value_length) == 0)
      {
        return curr;
      }
    }
    curr = curr->next;
  }
  return 0;
}

XML_Node* XML_Node::find_key(const char* key_name)
{
  u64       node_name_length = strlen(key_name);
  XML_Node* curr             = this->child;
  while (curr != 0)
  {
    if (curr->header.name_length == node_name_length && strncmp(curr->header.name, key_name, node_name_length) == 0)
    {
      return curr;
    }
    curr = curr->next;
  }
  assert(0 && "Couldn't find" && key_name);
}
XML_Tag* XML_Node::find_tag(const char* tag_name)
{

  XML_Tag*           tags            = this->header.tags;
  const unsigned int tag_name_length = strlen(tag_name);
  for (unsigned int i = 0; i < this->header.tag_count; i++)
  {
    XML_Tag* tag = &tags[i];
    if (tag->name_length == tag_name_length && strncmp(tag_name, tag->name, tag_name_length) == 0)
    {
      return tag;
    }
  }
  assert(0 && "Couldn't find" && tag_name);
}

void debug_tag(XML_Tag* tag)
{
  printf("%.*s=\"%.*s\" ", (i32)tag->name_length, tag->name, (i32)tag->value_length, tag->value);
}

void debug_xml_header(XML_Header* header)
{

  printf("<%.*s ", (i32)header->name_length, header->name);
  for (unsigned int i = 0; i < header->tag_count; i++)
  {
    debug_tag(&header->tags[i]);
  }
  printf(">\n");
}

void debug_xml_node(XML_Node* xml)
{
  if (xml == 0)
  {
    return;
  }
  XML_Header* header = &xml->header;
  debug_xml_header(header);
  if (xml->type == XML_PARENT)
  {
    XML_Node* child = xml->child;
    debug_xml_node(child);
    while (child->next)
    {
      debug_xml_node(child->next);
      child = child->next;
    }
  }
  else if (xml->content_length != 0)
  {
    printf("%.*s\n", (i32)xml->content_length, xml->content);
  }
  printf("</%.*s>\n", (i32)header->name_length, header->name);
}

void debug_xml(XML* xml)
{
  printf("<?");
  for (unsigned int i = 0; i < xml->version_and_encoding_length; i++)
  {
    debug_tag(&xml->version_and_encoding[i]);
  }
  printf("?>");
  debug_xml_node(&xml->head);
}

u32 get_child_count_by_name(XML_Node* node, const char* name)
{
  u32       count       = 0;
  XML_Node* child       = node->child;
  u32       name_length = strlen(name);
  while (child)
  {
    if (child->header.name_length == name_length && strncmp(name, child->header.name, name_length) == 0)
    {
      count++;
    }
    child = child->next;
  }

  return count;
}

static void write_xml_node(XML_Node* xml, FILE* ptr, int tabs)
{

  if (xml == 0)
  {
    return;
  }
  XML_Header* header = &xml->header;
  char        t[tabs];
  memset(t, '\t', tabs);
  fprintf(ptr, "%.*s<%.*s", tabs, t, (i32)header->name_length, header->name);
  for (unsigned int i = 0; i < header->tag_count; i++)
  {
    XML_Tag* tag = &header->tags[i];
    fprintf(ptr, " %.*s=\"%.*s\"", (i32)tag->name_length, tag->name, (i32)tag->value_length, tag->value);
  }
  if (xml->type == XML_CLOSED)
  {
    fprintf(ptr, "/>\n");
  }
  else if (xml->type == XML_PARENT)
  {
    fprintf(ptr, ">\n");

    XML_Node* child = xml->child;
    write_xml_node(child, ptr, tabs + 1);

    char t[tabs];
    memset(t, '\t', tabs);
    fprintf(ptr, "%.*s</%.*s>\n", tabs, t, (i32)header->name_length, header->name);
  }
  else if (xml->content_length != 0)
  {
    fprintf(ptr, ">%.*s</%.*s>\n", (i32)xml->content_length, xml->content, (i32)header->name_length, header->name);
  }
  else
  {
    char t[tabs];
    memset(t, '\t', tabs);
    fprintf(ptr, ">%.*s</%.*s>\n", tabs, t, (i32)header->name_length, header->name);
  }
  write_xml_node(xml->next, ptr, tabs);
}
static void write_xml(XML* xml, FILE* ptr)
{
  fprintf(ptr, "<?");
  for (unsigned int i = 0; i < xml->version_and_encoding_length; i++)
  {

    XML_Tag* tag = &xml->version_and_encoding[i];
    fprintf(ptr, " %.*s=\"%.*s\"", (i32)tag->name_length, tag->name, (i32)tag->value_length, tag->value);
  }
  fprintf(ptr, "?>\n");
  write_xml_node(&xml->head, ptr, 0);
}

void write_xml_to_file(XML* xml, const char* filename)
{
  FILE* file_ptr = fopen(filename, "w");
  write_xml(xml, file_ptr);
}

static bool compare_string(XML_Header* xml, Buffer* buffer, u64 index)
{
  u64 len = buffer->index - index;
  if (len != xml->name_length)
  {
    printf("Not the same length!\n");
    return false;
  }
  return strncmp(xml->name, &buffer->buffer[index], len) == 0;
}

enum XML_Header_Result
{
  XML_HEADER_OKAY,
  XML_HEADER_ERROR,
  XML_HEADER_CLOSED
};

bool parse_tag(Buffer* buffer, XML_Tag* current_tag)
{

  // parse name
  u64 index         = buffer->index;
  current_tag->name = &CURRENT_CHAR(buffer);

  // very robust yes
  SKIP(buffer, !match(buffer, '='));
  current_tag->name_length = buffer->index - index;

  ADVANCE(buffer);
  if (!match(buffer, '\"'))
  {
    printf("Expected '\"' for tag content?\n");
    return false;
  }
  ADVANCE(buffer);
  current_tag->value = &CURRENT_CHAR(buffer);
  index              = buffer->index;

  SKIP(buffer, CURRENT_CHAR(buffer) != '\"');

  current_tag->value_length = buffer->index - index;
  ADVANCE(buffer);
  return true;
}
XML_Header_Result parse_header(XML_Node* xml, Buffer* buffer)
{
  const int initial_tag_capacity = 8;
  // <NAME [TAG_NAME="TAG_CONTENT"]>
  XML_Header* header   = &xml->header;
  header->name         = &CURRENT_CHAR(buffer);
  header->tag_capacity = initial_tag_capacity;
  header->tags         = (XML_Tag*)sta_allocate(sizeof(XML_Tag) * initial_tag_capacity);
  u64 index            = buffer->index;
  SKIP(buffer, CURRENT_CHAR(buffer) != ' ' && CURRENT_CHAR(buffer) != '>' && CURRENT_CHAR(buffer) != '/');
  header->name_length = buffer->index - index;

  // parse tags
  for (;;)
  {
    skip_whitespace(buffer);
    if (isalpha(CURRENT_CHAR(buffer)))
    {
      RESIZE_ARRAY(header->tags, XML_Tag, header->tag_count, header->tag_capacity);
      if (!parse_tag(buffer, &header->tags[header->tag_count++]))
      {
        return XML_HEADER_ERROR;
      }
    }
    else if (match(buffer, '/') && NEXT_CHAR(buffer) == '>')
    {
      ADVANCE(buffer);
      ADVANCE(buffer);
      return XML_HEADER_CLOSED;
    }
    else if (match(buffer, '>'))
    {
      ADVANCE(buffer);
      return XML_HEADER_OKAY;
    }
    else
    {
      printf("Idk should close or alpha? %c\n", CURRENT_CHAR(buffer));
      return XML_HEADER_ERROR;
    }
  }
}

bool close_xml(XML_Node* xml, Buffer* buffer)
{

  ADVANCE(buffer);
  u64 index = buffer->index;
  SKIP(buffer, CURRENT_CHAR(buffer) != '>');

  if (!compare_string(&xml->header, buffer, index))
  {
    printf("Closed the wrong thing?\n");
    return false;
  }
  ADVANCE(buffer);
  return true;
}

bool parse_content(XML_Node* xml, Buffer* buffer, u64 index)
{
  xml->type    = XML_CONTENT;
  xml->content = &CURRENT_CHAR(buffer);
  SKIP(buffer, CURRENT_CHAR(buffer) != '<');

  xml->content_length = buffer->index - index;
  ADVANCE(buffer);
  if (!match(buffer, '/'))
  {
    debug_xml_node(xml);
    printf("Should be closing?\n");
    return false;
  }
  return close_xml(xml, buffer);
}

bool parse_xml(XML_Node* xml, Buffer* buffer)
{

  skip_whitespace(buffer);
  if (!match(buffer, '<'))
  {
    printf("Unknown char? %c %ld\n", buffer->buffer[buffer->index], buffer->index);
    return false;
  }
  ADVANCE(buffer);
  XML_Header_Result result = parse_header(xml, buffer);
  if (result == XML_HEADER_ERROR)
  {
    return false;
  }

  if (result == XML_HEADER_CLOSED)
  {
    xml->type = XML_CLOSED;
    return true;
  }

  skip_whitespace(buffer);
  u64 index = buffer->index;
  if (match(buffer, '<'))
  {
    // check empty
    if (NEXT_CHAR(buffer) == '/')
    {
      // advance past '<'
      ADVANCE(buffer);
      return close_xml(xml, buffer);
    }
    xml->type          = XML_PARENT;
    xml->child         = (XML_Node*)sta_allocate(sizeof(XML_Node));
    xml->child->parent = xml;
    if (!parse_xml(xml->child, buffer))
    {
      return false;
    }

    XML_Node* next = xml->child;
    for (;;)
    {
      skip_whitespace(buffer);
      if (!match(buffer, '<'))
      {
        return true;
      }
      if (NEXT_CHAR(buffer) == '/')
      {
        ADVANCE(buffer);
        return close_xml(xml, buffer);
      }
      next->next         = (XML_Node*)sta_allocate(sizeof(XML_Node));
      next->next->parent = xml;
      next               = next->next;
      if (!parse_xml(next, buffer))
      {
        return false;
      }
    }
  }
  return parse_content(xml, buffer, index);
}
bool parse_version_and_encoding(XML* xml, Buffer* buffer)
{
  skip_whitespace(buffer);
  // parse header?
  if (!match(buffer, '<'))
  {
    printf("Unknown char? %c %ld\n", buffer->buffer[buffer->index], buffer->index);
    return false;
  }
  ADVANCE(buffer);
  if (!match(buffer, '?'))
  {
    printf("Tried to parse xml version thingy but didn't find it?\n");
    return false;
  }
  ADVANCE(buffer);
  if (match(buffer, '?') && NEXT_CHAR(buffer) == '>')
  {
    ADVANCE(buffer);
    ADVANCE(buffer);
    return true;
  }

  const int initial_tag_capacity     = 2;
  xml->version_and_encoding_capacity = initial_tag_capacity;
  xml->version_and_encoding          = (XML_Tag*)sta_allocate(sizeof(XML_Tag) * initial_tag_capacity);
  do
  {
    skip_whitespace(buffer);
    RESIZE_ARRAY(xml->version_and_encoding, XML_Tag, xml->version_and_encoding_length, xml->version_and_encoding_capacity);
    if (!parse_tag(buffer, &xml->version_and_encoding[xml->version_and_encoding_length++]))
    {
      return false;
    }
  } while (!match(buffer, '?'));
  ADVANCE(buffer);
  if (!match(buffer, '>'))
  {
    return false;
  }
  ADVANCE(buffer);

  return true;
}
static void parse_vector2_array(Buffer* buffer, Vector2* vectors, u64 count)
{
  for (unsigned int i = 0; i < count; i++)
  {
    Vector2* position = &vectors[i];
    skip_whitespace(buffer);
    position->x = parse_float_from_string(buffer);
    skip_whitespace(buffer);
    position->y = parse_float_from_string(buffer);
  }
}

static void parse_vector3_array(Buffer* buffer, Vector3* vectors, u64 count)
{
  for (unsigned int i = 0; i < count; i++)
  {
    Vector3* position = &vectors[i];
    skip_whitespace(buffer);
    position->x = parse_float_from_string(buffer);
    skip_whitespace(buffer);
    position->y = parse_float_from_string(buffer);
    skip_whitespace(buffer);
    position->z = parse_float_from_string(buffer);
  }
}

void parse_int_array(i32* array, u64 count, Buffer* buffer)
{
  for (unsigned int i = 0; i < count; i++)
  {
    while (!(isdigit(CURRENT_CHAR(buffer)) || CURRENT_CHAR(buffer) == '-'))
    {
      ADVANCE(buffer);
    }
    array[i] = parse_int_from_string(buffer);
  }
}

void parse_float_array(f32* array, u64 count, Buffer* buffer, const char* joint, i32 joint_length)
{
  printf("%.*s\n", joint_length, joint);
  printf("Parsing from %.*s\n", (i32)buffer->len, buffer->buffer);
  for (unsigned int i = 0; i < count; i++)
  {
    while (!(isdigit(CURRENT_CHAR(buffer)) || CURRENT_CHAR(buffer) == '-' || CURRENT_CHAR(buffer) == 'e'))
    {
      ADVANCE(buffer);
    }
    array[i] = parse_float_from_string(buffer);
    printf("%f\n", array[i]);
  }
  printf("--\n");
}
void parse_float_array(f32* array, u64 count, Buffer* buffer)
{
  for (unsigned int i = 0; i < count; i++)
  {
    while (!buffer->is_out_of_bounds() && !(isdigit(CURRENT_CHAR(buffer)) || CURRENT_CHAR(buffer) == '-' || CURRENT_CHAR(buffer) == 'e'))
    {
      ADVANCE(buffer);
    }
    if (buffer->is_out_of_bounds())
    {
      assert(0 && "Trying to parse outside?");
    }
    array[i] = parse_float_from_string(buffer);
  }
}

static int sta_xml_get_count_tag(XML_Node* node)
{
  XML_Tag* node_tag = node->find_tag("count");
  Buffer   buffer   = {};
  buffer.index      = 0;
  buffer.buffer     = (char*)node_tag->value;
  buffer.len        = node_tag->value_length;
  return parse_int_from_string(&buffer);
}

static u8 get_node_index_from_id(const char** names, u32 count, const char* name, u64 length)
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
static u8 get_node_index_from_id(Skeleton* skeleton, const char* name, u64 length)
{
  for (unsigned int i = 0; i < skeleton->joint_count; i++)
  {
    if (strlen(skeleton->joints[i].m_name) == length && strncmp(name, skeleton->joints[i].m_name, length) == 0)
    {
      return i;
    }
  }
  return -1;
}

struct JointData
{
  int         index;
  const char* name;
  u32         name_length;
  Mat44       local_bind_transform;
  Mat44       inverse_bind_transform;
  JointData*  children;
  u32         children_cap;
  u32         children_count;
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
  const char**    joint_order;
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

struct AnimationData
{
  KeyFrameData* key_frames;
  float*        timesteps;
  float         duration;
  u32           count;
};

struct AnimationModelData
{
  SkeletonData  skeleton_data;
  MeshData      mesh_data;
  SkinningData  skinning_data;
  AnimationData animation_data;
};

void parse_string_array(char** array, u32 count, const char* content, u32 content_length)
{
  u32 idx = 0, prev = 0;
  for (u32 i = 0; i < count; i++)
  {
    while (content[idx] != ' ' && content[idx] != '<')
    {
      idx++;
    }
    array[i] = (char*)sta_allocate(idx - prev + 1);
    strncpy(array[i], &content[prev], idx - prev);
    array[i][idx - prev] = '\0';

    idx++;
    prev = idx;
  }
}

int cmp_vertex_skin_data(const void* _a, const void* _b)
{
  JointWeight* a = (JointWeight*)_a;
  JointWeight* b = (JointWeight*)_b;
  return a->weight - b->weight;
}

void get_skin_data(VertexSkinData* vertex_skin_data, u32 vertex_skin_count, i32* vcount, i32 vcount_count, f32* weights, XML_Node* vertex_weights)
{
  XML_Node* v    = vertex_weights->find_key("v");
  Buffer    buff = {};
  buff.len       = v->content_length;
  buff.buffer    = (char*)v->content;
  Buffer* buffer = &buff;

  for (u32 i = 0; i < vcount_count; i++)
  {
    VertexSkinData skin_data = {};
    i32            vc        = vcount[i];
    skin_data.count          = MIN(vc, 4);
    skin_data.data           = (JointWeight*)sta_allocate(sizeof(JointWeight) * skin_data.count);
    for (u32 j = 0; j < vc; j++)
    {
      skip_whitespace(buffer);
      i32 joint_id = parse_int_from_string(buffer);
      skip_whitespace(buffer);
      i32 weight_id = parse_int_from_string(buffer);
      if (j < 4)
      {
        skin_data.data[j].weight   = weights[weight_id];
        skin_data.data[j].joint_id = joint_id;
      }
    }

    qsort(&skin_data, sizeof(JointWeight), vc, cmp_vertex_skin_data);

    if (vc > 4)
    {

      float sum = 0.0f;
      for (u32 j = 0; j < 4; j++)
      {
        sum += skin_data.data[j].weight;
      }
      float after_sum = 0.0f;
      for (u32 j = 0; j < 4; j++)
      {
        skin_data.data[j].weight /= sum;
        after_sum += skin_data.data[j].weight;
      }
    }

    vertex_skin_data[i] = skin_data;
  }
}

void debug_skinning_data(SkinningData* skinning_data)
{
  for (u32 i = 0; i < skinning_data->joint_order_count; i++)
  {
    printf("%s\n", skinning_data->joint_order[i]);
  }
  for (u32 i = 0; i < skinning_data->vertex_skin_data_count; i++)
  {
    VertexSkinData data = skinning_data->vertex_skin_data[i];
    printf("[");
    u32 j = 0;
    for (; j < data.count; j++)
    {
      printf("%d, ", data.data[j].joint_id);
    }
    for (; j < 4; j++)
    {
      printf("0, ");
    }
    printf("]\n[");
    for (j = 0; j < data.count; j++)
    {
      printf("%f, ", data.data[j].weight);
    }
    for (; j < 4; j++)
    {
      printf("0, ");
    }
    printf("]\n");
  }
}

void extract_skin_data(SkinningData* skinning_data, XML_Node* controllers)
{
  XML_Node* skin           = controllers->find_key("controller")->find_key("skin");
  XML_Node* vertex_weights = skin->find_key("vertex_weights");
  XML_Tag*  joint_id_tag   = vertex_weights->find_key_with_tag("input", "semantic", "JOINT")->find_tag("source");

  char*     joint_id       = (char*)sta_allocate(joint_id_tag->value_length);
  strncpy((char*)joint_id, &joint_id_tag->value[1], joint_id_tag->value_length - 1);
  joint_id[joint_id_tag->value_length - 1] = '\0';

  XML_Node* joint_node                     = skin->find_key_with_tag("source", "id", joint_id)->find_key("Name_array");

  u32       count                          = sta_xml_get_count_tag(joint_node);
  skinning_data->joint_order               = (const char**)sta_allocate(sizeof(const char*) * count);
  skinning_data->joint_order_count         = count;
  parse_string_array((char**)skinning_data->joint_order, count, joint_node->content, joint_node->content_length);

  sta_deallocate(joint_id, joint_id_tag->value_length);

  XML_Tag* weights_id_tag = vertex_weights->find_key_with_tag("input", "semantic", "WEIGHT")->find_tag("source");
  char*    weights_id     = (char*)sta_allocate(weights_id_tag->value_length);
  strncpy((char*)weights_id, &weights_id_tag->value[1], weights_id_tag->value_length - 1);
  weights_id[weights_id_tag->value_length - 1] = '\0';

  XML_Node* weight_node                        = skin->find_key_with_tag("source", "id", weights_id)->find_key("float_array");
  u32       weight_count                       = sta_xml_get_count_tag(weight_node);
  float*    weights                            = (f32*)sta_allocate(sizeof(f32) * weight_count);
  Buffer    buffer                             = {};
  buffer.buffer                                = (char*)weight_node->content;
  buffer.len                                   = weight_node->content_length;
  parse_float_array(weights, weight_count, &buffer);
  sta_deallocate(weights_id, weights_id_tag->value_length);

  u32 vertex_weights_count              = sta_xml_get_count_tag(vertex_weights);
  skinning_data->vertex_skin_data_count = vertex_weights_count;
  skinning_data->vertex_skin_data       = (VertexSkinData*)sta_allocate(sizeof(VertexSkinData) * vertex_weights_count);
  i32*      vcount                      = (i32*)sta_allocate(sizeof(i32) * vertex_weights_count);
  XML_Node* vcount_node                 = vertex_weights->find_key("vcount");
  buffer.buffer                         = (char*)vcount_node->content;
  buffer.len                            = vcount_node->content_length;
  buffer.index                          = 0;
  parse_int_array(vcount, vertex_weights_count, &buffer);

  get_skin_data(skinning_data->vertex_skin_data, vertex_weights_count, vcount, vertex_weights_count, weights, vertex_weights);
}

static Mat44 convert_z_to_y_up()
{
  Mat44 res;
  res.identity();
  res = res.rotate_x(-90);
  return res.transpose();
}

static Mat44 CORRECTION = convert_z_to_y_up();

void         extract_main_joint_data(JointData* joint, XML_Node* child, bool is_root, const char** bone_order, u32 bone_count)
{
  XML_Tag* name_tag     = child->find_tag("id");
  u32      idx          = get_node_index_from_id(bone_order, bone_count, name_tag->value, name_tag->value_length);

  joint->name           = name_tag->value;
  joint->name_length    = name_tag->value_length;
  joint->index          = idx;

  XML_Node* matrix_node = child->find_key("matrix");
  Buffer    buffer      = {};
  buffer.buffer         = (char*)matrix_node->content;
  buffer.len            = matrix_node->content_length;

  parse_float_array(&joint->local_bind_transform.m[0], 16, &buffer);
  joint->local_bind_transform = joint->local_bind_transform.transpose();
  if (is_root)
  {
    joint->local_bind_transform = CORRECTION.mul(joint->local_bind_transform);
  }
  printf("%.*s\n", joint->name_length, joint->name);
  joint->local_bind_transform.debug();
  printf("--\n");
}

void load_joint_data(JointData* joint, XML_Node* node, bool is_root, const char** bone_order, u32 bone_count)
{
  const int length_of_node = 4;
  extract_main_joint_data(joint, node, is_root, bone_order, bone_count);

  XML_Node* child_node = node->child;
  while (child_node)
  {
    if (child_node->header.name_length == length_of_node && strncmp(child_node->header.name, "node", 4) == 0)
    {
      RESIZE_ARRAY(joint->children, JointData, joint->children_count, joint->children_cap);
      JointData* child_joint      = &joint->children[joint->children_count++];
      child_joint->children_cap   = 1;
      child_joint->children       = (JointData*)sta_allocate(sizeof(JointData));
      child_joint->children_count = 0;
      load_joint_data(child_joint, child_node, false, bone_order, bone_count);
    }

    child_node = child_node->next;
  }
}

void debug_joint(JointData* joint)
{
  printf("%.*s %d\n", (i32)joint->name_length, joint->name, joint->index);
  printf("Inverse bind:\n");
  joint->inverse_bind_transform.debug();
  printf("---\n");
  printf("Local:\n");
  joint->local_bind_transform.debug();
  printf("---\n");
  for (u32 i = 0; i < joint->children_count; i++)
  {
    debug_joint(&joint->children[i]);
  }
}

void debug_skeleton_data(SkeletonData* skeleton_data)
{
  printf("SKELETON\n");
  printf("Joint Count: %d\n", skeleton_data->joint_count);
  debug_joint(skeleton_data->head_joint);
}

void extract_bone_data(SkeletonData* skeleton_data, XML_Node* visual_scenes, const char** bone_order, u32 bone_count)
{
  XML_Node* armature_data                   = visual_scenes->find_key("visual_scene")->find_key_with_tag("node", "id", "Armature");
  skeleton_data->joint_count                = bone_count;
  XML_Node* child                           = armature_data->find_key("node");
  skeleton_data->head_joint                 = (JointData*)sta_allocate(sizeof(JointData));
  skeleton_data->head_joint->children_cap   = 1;
  skeleton_data->head_joint->children       = (JointData*)sta_allocate(sizeof(JointData));
  skeleton_data->head_joint->children_count = 0;
  load_joint_data(skeleton_data->head_joint, child, true, bone_order, bone_count);
}

void read_positions(XML_Node* mesh_node, Vector3** positions, u32& count, VertexSkinData* skin_data)
{

  XML_Tag* source_tag   = mesh_node->find_key("vertices")->find_key("input")->find_tag("source");
  char*    positions_id = (char*)sta_allocate(source_tag->value_length);
  strncpy((char*)positions_id, &source_tag->value[1], source_tag->value_length - 1);
  positions_id[source_tag->value_length - 1] = '\0';

  XML_Node* positions_data                   = mesh_node->find_key_with_tag("source", "id", positions_id)->find_key("float_array");

  count                                      = sta_xml_get_count_tag(positions_data);
  Vector3* pos                               = (Vector3*)sta_allocate(sizeof(Vector3) * count);
  Buffer   buffer                            = {};
  buffer.buffer                              = (char*)positions_data->content;
  buffer.len                                 = positions_data->content_length;

  parse_vector3_array(&buffer, pos, count);

  for (int i = 0; i < count; i++)
  {
    Vector3* v  = &pos[i];

    Vector4  v4 = Vector4(v->x, v->y, v->z, 1.0f);
    v4          = CORRECTION.mul(v4);
    v->x        = v4.x;
    v->y        = v4.y;
    v->z        = v4.z;
  }
  *positions = pos;
}

void read_normals(XML_Node* mesh_node, Vector3** normals, u32& count)
{
  XML_Tag* source_tag = mesh_node->find_key("polylist")->find_key_with_tag("input", "semantic", "NORMAL")->find_tag("source");
  char*    normal_id  = (char*)sta_allocate(source_tag->value_length);
  strncpy((char*)normal_id, &source_tag->value[1], source_tag->value_length - 1);
  normal_id[source_tag->value_length - 1] = '\0';

  XML_Node* normals_node                  = mesh_node->find_key_with_tag("source", "id", normal_id)->find_key("float_array");

  count                                   = sta_xml_get_count_tag(normals_node);
  Vector3* norm                           = (Vector3*)sta_allocate(sizeof(Vector3) * count);
  Buffer   buffer                         = {};
  buffer.buffer                           = (char*)normals_node->content;
  buffer.len                              = normals_node->content_length;

  parse_vector3_array(&buffer, norm, count);
  // ToDo this needs to be tranformed since z up :)

  *normals = norm;
}

void read_texture_coords(XML_Node* mesh_node, Vector2** uv, u32& count)
{
  XML_Tag* source_tag = mesh_node->find_key("polylist")->find_key_with_tag("input", "semantic", "TEXCOORD")->find_tag("source");
  char*    uv_id      = (char*)sta_allocate(source_tag->value_length);
  strncpy((char*)uv_id, &source_tag->value[1], source_tag->value_length - 1);
  uv_id[source_tag->value_length - 1] = '\0';

  XML_Node* uv_node                   = mesh_node->find_key_with_tag("source", "id", uv_id)->find_key("float_array");
  count                               = sta_xml_get_count_tag(uv_node);
  Vector2* v                          = (Vector2*)sta_allocate(sizeof(Vector2) * count);
  Buffer   buffer                     = {};
  buffer.buffer                       = (char*)uv_node->content;
  buffer.len                          = uv_node->content_length;

  parse_vector2_array(&buffer, v, count);

  *uv = v;
}

void extract_model_data(MeshData* model, XML_Node* geometry, VertexSkinData* skin_data)
{

  XML_Node* mesh_node = geometry->find_key("geometry")->find_key("mesh");

  Vector3*  positions = {};
  u32       position_count;
  read_positions(mesh_node, &positions, position_count, skin_data);

  Vector3* normals = {};
  u32      normal_count;
  read_normals(mesh_node, &normals, normal_count);

  Vector2* uv = {};
  u32      uv_count;
  read_texture_coords(mesh_node, &uv, uv_count);

  XML_Node* poly       = mesh_node->find_key("polylist");
  int       faces      = sta_xml_get_count_tag(poly) * 3;
  int       type_count = get_child_count_by_name(poly, "input");

  XML_Node* p          = poly->find_key("p");

  Buffer    buffer     = {};
  buffer.len           = p->content_length;
  buffer.buffer        = (char*)p->content;
  model->vertex_count  = faces;
  model->indices       = (u32*)sta_allocate(sizeof(u32) * faces);
  model->vertices      = (SkinnedVertex*)sta_allocate(sizeof(SkinnedVertex) * faces);
  for (int i = 0; i < faces; i++)
  {

    skip_whitespace(&buffer);
    u32 position_index = parse_int_from_string(&buffer);
    skip_whitespace(&buffer);
    u32 normal_index = parse_int_from_string(&buffer);
    skip_whitespace(&buffer);
    u32 uv_index = parse_int_from_string(&buffer);

    for (u32 j = 0; j < type_count - 3; j++)
    {
      skip_whitespace(&buffer);
      (void)parse_int_from_string(&buffer);
    }

    model->indices[i]     = i;
    SkinnedVertex* vertex = &model->vertices[i];
    vertex->normal        = normals[normal_index];
    vertex->position      = positions[position_index];
    vertex->uv            = uv[uv_index];

    VertexSkinData* sd    = &skin_data[position_index];
    for (u32 j = 0; j < sd->count; j++)
    {
      vertex->joint_index[j]  = sd->data[j].joint_id;
      vertex->joint_weight[j] = sd->data[j].weight;
    }
  }
}

void calc_inv_bind(JointData* joint, Mat44 parent_transform)
{
  Mat44 bind_transform          = parent_transform.mul(joint->local_bind_transform);
  joint->inverse_bind_transform = bind_transform.inverse();
  for (u32 i = 0; i < joint->children_count; i++)
  {
    calc_inv_bind(&joint->children[i], bind_transform);
  }
}

void calculate_inverse_bind_transforms(SkeletonData* skeleton_data)
{
  Mat44 m = {};
  m.identity();
  calc_inv_bind(skeleton_data->head_joint, m);
}

void load_collada_model(AnimationModelData* data, XML_Node* head)
{
  XML_Node* controller = head->find_key("library_controllers");
  extract_skin_data(&data->skinning_data, controller);

  XML_Node* geometry_node = head->find_key("library_geometries");
  extract_model_data(&data->mesh_data, geometry_node, data->skinning_data.vertex_skin_data);

  XML_Node* visual_scene = head->find_key("library_visual_scenes");
  extract_bone_data(&data->skeleton_data, visual_scene, data->skinning_data.joint_order, data->skinning_data.joint_order_count);

  calculate_inverse_bind_transforms(&data->skeleton_data);
}

void transfer_joints(Skeleton* skeleton, JointData* joint_data, u32 parent)
{
  Joint* joint         = &skeleton->joints[joint_data->index];
  joint->m_iParent     = parent;
  joint->m_mat         = joint_data->local_bind_transform;
  joint->m_invBindPose = joint_data->inverse_bind_transform;
  joint->m_name        = (char*)joint_data->name;
  joint->m_name_length = joint_data->name_length;
  for (u32 i = 0; i < joint_data->children_count; i++)
  {
    transfer_joints(skeleton, &joint_data->children[i], joint_data->index);
  }
}

void normalize_positions(SkinnedVertex* vertices, u32 count, f32& low, f32& high)
{
  for (u32 i = 0; i < count; i++)
  {
    Vector3 pos = vertices[i].position;
    low         = MIN(pos.x, low);
    low         = MIN(pos.y, low);
    low         = MIN(pos.z, low);

    high        = MAX(pos.x, high);
    high        = MAX(pos.y, high);
    high        = MAX(pos.z, high);
  }

  float diff = high - low;
  for (u32 i = 0; i < count; i++)
  {
    Vector3* pos = &vertices[i].position;
    // pos->x       = ((pos->x - low) / diff) * 2.0f - 1.0f;
    // pos->y       = ((pos->y - low) / diff) * 2.0f - 1.0f;
    // pos->z       = ((pos->z - low) / diff) * 2.0f - 1.0f;
  }
}

void find_root_joint_name(XML_Node* visual_scenes_node, char** name, u32& length)
{
  XML_Node* skeleton = visual_scenes_node->find_key("visual_scene")->find_key_with_tag("node", "id", "Armature");
  XML_Tag*  node     = skeleton->find_key("node")->find_tag("id");
  *name              = (char*)node->value;
  length             = node->value_length;
}

void get_key_times(XML_Node* node, f32** timesteps, u32& length)
{

  XML_Node* time_data = node->find_key("animation")->find_key("source")->find_key("float_array");

  Buffer    buffer((char*)time_data->content, time_data->content_length);

  length     = sta_xml_get_count_tag(time_data);
  f32* times = (f32*)sta_allocate(sizeof(f32) * length);
  parse_float_array(times, length, &buffer);
  *timesteps = times;
}

void get_joint_name(char** joint_name, u32& length, XML_Node* joint_node)
{
  XML_Tag* tag = joint_node->find_key("channel")->find_tag("target");
  Buffer   buffer((char*)tag->value, tag->value_length);
  Buffer*  buf = &buffer;
  SKIP(buf, CURRENT_CHAR(buf) != '/');

  char* name       = (char*)sta_allocate(sizeof(char) * (buf->index + 1));
  name[buf->index] = '\0';
  strncpy(name, buffer.buffer, buf->index);

  *joint_name = name;
  length      = buf->index;
}

void get_joint_id(char** joint_id, u32& length, XML_Node* joint_node)
{
  XML_Tag* source_tag = joint_node->find_key("sampler")->find_key_with_tag("input", "semantic", "OUTPUT")->find_tag("source");
  *joint_id           = (char*)sta_allocate(source_tag->value_length);
  strncpy((char*)(*joint_id), &source_tag->value[1], source_tag->value_length - 1);
  (*joint_id)[source_tag->value_length - 1] = '\0';
  length                                    = source_tag->value_length;
}

void load_joint_transforms(AnimationData* animation_data, XML_Node* joint_node, u32 joint_transform_index, char* root_name, u32 root_name_length)
{
  char* joint_name;
  u32   joint_name_length;
  get_joint_name(&joint_name, joint_name_length, joint_node);

  char* data_id;
  u32   data_id_length;
  get_joint_id(&data_id, data_id_length, joint_node);

  XML_Node* transform_data = joint_node->find_key_with_tag("source", "id", data_id)->find_key("float_array");
  Buffer    buffer((char*)transform_data->content, transform_data->content_length);

  for (u32 i = 0; i < animation_data->count; i++)
  {
    KeyFrameData*       frame_data = &animation_data->key_frames[i];
    JointTransformData* transform  = &frame_data->transforms[joint_transform_index];

    parse_float_array(&transform->local_transform.m[0], 16, &buffer);
    transform->local_transform = transform->local_transform.transpose();
    printf("%d vs %d, %.*s vs %.*s\n", joint_name_length, root_name_length, joint_name_length, joint_name, root_name_length, root_name);
    if (joint_name_length == root_name_length && strncmp(joint_name, root_name, root_name_length) == 0)
    {
      transform->local_transform = CORRECTION.mul(transform->local_transform);
    }
  }
}

void load_animation_data(AnimationModelData* model_data, XML_Node* head)
{
  XML_Node*      animation_node     = head->find_key("library_animations");
  XML_Node*      visual_scenes_node = head->find_key("library_visual_scenes");

  char*          root_name;
  u32            root_name_length;

  AnimationData* animation_data = &model_data->animation_data;

  find_root_joint_name(visual_scenes_node, &root_name, root_name_length);
  get_key_times(animation_node, &animation_data->timesteps, animation_data->count);
  animation_data->duration   = animation_data->timesteps[animation_data->count - 1];

  animation_data->key_frames = (KeyFrameData*)sta_allocate(sizeof(KeyFrameData) * animation_data->count);
  for (u32 i = 0; i < animation_data->count; i++)
  {
    animation_data->key_frames[i].time       = animation_data->timesteps[i];
    animation_data->key_frames[i].transforms = (JointTransformData*)sta_allocate(sizeof(JointTransformData) * model_data->skeleton_data.joint_count);
  }

  XML_Node* joint_node       = animation_node->child;
  u32       node_name_length = strlen("animation");
  u32       frame_index      = 0;

  while (joint_node)
  {
    if (joint_node->header.name_length == node_name_length && strncmp(joint_node->header.name, "animation", node_name_length) == 0)
    {
      load_joint_transforms(animation_data, joint_node, frame_index, root_name, root_name_length);
      frame_index++;
    }

    joint_node = joint_node->next;
  }
}

bool sta_collada_parse_from_file(AnimationModel* animation, const char* filename)
{

  XML    xml    = {};
  Buffer buffer = {};
  if (!sta_read_file(&buffer, filename))
  {
    return 0;
  }

  if (!parse_version_and_encoding(&xml, &buffer))
  {
    return false;
  }

  if (!parse_xml(&xml.head, &buffer))
  {
    return false;
  };

  AnimationModelData animation_model = {};
  load_collada_model(&animation_model, &xml.head);

  animation->vertices             = animation_model.mesh_data.vertices;
  animation->indices              = animation_model.mesh_data.indices;
  animation->vertex_count         = animation_model.mesh_data.vertex_count;
  animation->skeleton.joint_count = animation_model.skeleton_data.joint_count;
  animation->skeleton.joints      = (Joint*)sta_allocate(sizeof(Joint) * animation->skeleton.joint_count);

  transfer_joints(&animation->skeleton, animation_model.skeleton_data.head_joint, 0);
  for (u32 i = 0; i < animation->skeleton.joint_count; i++)
  {
    Joint* joint = &animation->skeleton.joints[i];
  }

  float low = FLT_MAX, high = -FLT_MAX;
  normalize_positions(animation->vertices, animation->vertex_count, low, high);

  load_animation_data(&animation_model, &xml.head);

  animation->animations.pose_count = animation_model.animation_data.count;
  animation->animations.duration   = animation_model.animation_data.duration;
  animation->animations.poses      = (JointPose*)sta_allocate(sizeof(JointPose) * animation->animations.pose_count);

  // keyframe are how many different poses there are in a animation
  // transforms are one for each joint
  Animation* anim = &animation->animations;
  for (u32 i = 0; i < animation->animations.pose_count; i++)
  {
    JointPose* pose       = &animation->animations.poses[i];
    pose->local_transform = (Mat44*)sta_allocate(sizeof(Mat44) * animation_model.skeleton_data.joint_count);
    pose->names           = (String*)sta_allocate(sizeof(String) * animation_model.skeleton_data.joint_count);

    for (u32 j = 0; j < animation_model.skeleton_data.joint_count; j++)
    {
      KeyFrameData* frame      = &animation_model.animation_data.key_frames[i];
      pose->local_transform[j] = frame->transforms[j].local_transform;
      pose->names[j].s         = animation->skeleton.joints[j].m_name;
      pose->names[j].l         = animation->skeleton.joints[j].m_name_length;
    }
  }

  return true;
}
