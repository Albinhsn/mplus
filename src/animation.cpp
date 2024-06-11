#include "animation.h"
#include "common.h"
#include "files.h"
#include "platform.h"
#include "vector.h"
#include <cassert>
#include <cctype>
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

void JointPose::debug()
{
  printf("JointPose %s\n", this->name);
  for (u32 i = 0; i < this->steps; i++)
  {
    printf("Time: %f\n", this->t[i]);
    this->local_transform[i].debug();
    printf("--\n");
  }
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

  printf("Animations:\n");
  for (u32 i = 0; i < this->animations.pose_count; i++)
  {
    this->animations.poses[i].debug();
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
  return 0;
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
  return 0;
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

void parse_float_array(f32* array, u64 count, Buffer* buffer)
{
  for (unsigned int i = 0; i < count; i++)
  {
    while (!(isdigit(CURRENT_CHAR(buffer)) || CURRENT_CHAR(buffer) == '-'))
    {
      ADVANCE(buffer);
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
  u32 weight_idx;
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
  f32* vertices;
  f32* normals;
  f32* uv;
  u32* indices;
  u32* joint_ids;
  f32* weights;
};

struct AnimationModelData
{
  SkeletonData skeleton;
  MeshData     mesh;
};

struct Vertex
{
  Vector3        position;
  i32            texture_index;
  i32            normal_index;
  i32            idx;
  VertexSkinData weights_data;
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
        skin_data.data[j].weight     = weights[weight_id];
        skin_data.data[j].joint_id   = joint_id;
        skin_data.data[j].weight_idx = weight_id;
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
    for (u32 j = 0; j < data.count; j++)
    {
      printf("%d, ", data.data[j].joint_id);
    }
    printf("]\n[");
    float sum = 0.0f;
    for (u32 j = 0; j < data.count; j++)
    {
      printf("(%f, %d), ", data.data[j].weight, data.data[j].weight_idx);
      sum += data.data[j].weight;
    }
    printf("] %f\n", sum);
  }
}

void extract_skin_data(SkinningData* skinning_data, XML_Node* controllers)
{
  printf("Extracting skin data!\n");
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

  printf("Extracted skin data!\n");
  debug_skinning_data(skinning_data);
}

static Mat44 convert_z_to_y_up()
{
  Mat44 res;
  res.identity();
  res.rotate_x(-90);
  return res;
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
    joint->local_bind_transform = joint->local_bind_transform.mul(CORRECTION);
  }
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
  joint->local_bind_transform.debug();
  for (u32 i = 0; i < joint->children_count; i++)
  {
    debug_joint(&joint->children[i]);
  }
  printf("----\n");
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

  debug_skeleton_data(skeleton_data);
}

void read_positions(XML_Node* mesh_node, Vector3** positions, u32& count)
{
  XML_Tag* source_tag   = mesh_node->find_key("vertices")->find_key("input")->find_tag("source");
  char*    positions_id = (char*)sta_allocate(source_tag->value_length);
  strncpy((char*)positions_id, &source_tag->value[1], source_tag->value_length - 1);
  positions_id[source_tag->value_length - 1] = '\0';

  XML_Node* positions_data                   = mesh_node->find_key_with_tag("source", "id", positions_id);

  count                                      = sta_xml_get_count_tag(positions_data);
  Vector3* pos                               = (Vector3*)sta_allocate(sizeof(Vector3) * count);
  Buffer   buffer                            = {};
  buffer.buffer                              = (char*)positions_data->content;
  buffer.len                                 = positions_data->content_length;

  parse_vector3_array(&buffer, pos, count);
  *positions = pos;
}

void read_normals(XML_Node* mesh_node, Vector3** normals, u32& count)
{
  XML_Tag* source_tag = mesh_node->find_key("polylist")->find_key_with_tag("input", "semantic", "NORMAL")->find_tag("source");
  char*    normal_id  = (char*)sta_allocate(source_tag->value_length);
  strncpy((char*)normal_id, &source_tag->value[1], source_tag->value_length - 1);
  normal_id[source_tag->value_length - 1] = '\0';

  XML_Node* normals_node                  = mesh_node->find_key_with_tag("source", "id", normal_id);

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

  XML_Node* uv_node                   = mesh_node->find_key_with_tag("source", "id", uv_id);
  count                               = sta_xml_get_count_tag(uv_node);
  Vector2* v                          = (Vector2*)sta_allocate(sizeof(Vector2) * count);
  Buffer   buffer                     = {};
  buffer.buffer                       = (char*)uv_node->content;
  buffer.len                          = uv_node->content_length;

  parse_vector2_array(&buffer, v, count);

  *uv = v;
}

void extract_model_data(MeshData* model, XML_Node* geometry)
{

  XML_Node* mesh_node = geometry->find_key("geometry")->find_key("mesh");

  Vector3*  positions = {};
  u32       position_count;
  read_positions(mesh_node, &positions, position_count);

  Vector3* normals = {};
  u32      normal_count;
  read_positions(mesh_node, &normals, normal_count);

  Vector3* uv = {};
  u32      uv_count;
  read_positions(mesh_node, &uv, uv_count);

  XML_Node* poly       = mesh_node->find_key("polylist");

  int       type_count = get_child_count_by_name(poly, "input");

  Buffer    buffer     = {};
  for (int i = 0; i < type_count / 3; i++)
  {

    skip_whitespace(&buffer);
    u32 position_index = parse_int_from_string(&buffer);
    skip_whitespace(&buffer);
    u32 normal_index = parse_int_from_string(&buffer);
    skip_whitespace(&buffer);
    u32 uv_index = parse_int_from_string(&buffer);

    for (u32 j = 3; j < type_count; j++)
    {
      skip_whitespace(&buffer);
      (void)parse_int_from_string(&buffer);
    }
  }
}

void load_collada_model(AnimationModelData* data, XML_Node* head)
{
  XML_Node*    controller    = head->find_key("library_controllers");
  SkinningData skinning_data = {};
  extract_skin_data(&skinning_data, controller);

  MeshData  mesh          = {};
  XML_Node* geometry_node = head->find_key("library_geometries");
  extract_model_data(&mesh, geometry_node);

  XML_Node*    visual_scene  = head->find_key("library_visual_scenes");
  SkeletonData skeleton_data = {};
  extract_bone_data(&skeleton_data, visual_scene, skinning_data.joint_order, skinning_data.joint_order_count);
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

  // load collada model
  //  joints and mesh data

  XML_Node* animations = xml.head.find_key("library_animations");

  return true;
}
