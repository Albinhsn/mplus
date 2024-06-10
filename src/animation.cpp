#include "animation.h"
#include "files.h"
#include "platform.h"
#include <cctype>
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

bool sta_collada_parse_model_data(ColladaGeometry* geometry, XML_Node* node)
{
  XML_Node* geometry_node = node->find_key("geometry");
  if (geometry_node == 0)
  {
    printf("Failed to find geometry\n");
    return false;
  }
  XML_Node* mesh = geometry_node->find_key("mesh");
  if (mesh == 0)
  {
    printf("Failed to find mesh\n");
    return false;
  }
  XML_Node* position_source = mesh->find_key("source");
  if (position_source == 0)
  {
    printf("Failed to find position source\n");
    return false;
  }
  XML_Node* position_array = position_source->find_key("float_array");
  if (position_array == 0)
  {
    printf("Failed to find position array\n");
    return false;
  }
  XML_Tag* position_count_tag = position_array->find_tag("count");
  if (position_count_tag == 0)
  {
    printf("Failed to find count\n");
    return false;
  }

  int position_count = parse_int_from_string(position_count_tag->value) / 3;
  geometry->vertices = (Vector3*)sta_allocate(sizeof(Vector3) * position_count);
  Buffer buffer      = sta_xml_create_buffer_from_content(position_array);
  parse_vector3_array(&buffer, geometry->vertices, position_count);

  XML_Node* normal_source = position_source->next;
  if (normal_source == 0)
  {
    printf("Failed to find normal source\n");
    return false;
  }
  XML_Node* normal_array = normal_source->find_key("float_array");
  if (normal_array == 0)
  {
    printf("Failed to find position array\n");
    return false;
  }
  XML_Tag* normal_count_tag = normal_array->find_tag("count");
  if (normal_count_tag == 0)
  {
    printf("Failed to find normal count\n");
    return false;
  }
  int normal_count  = parse_int_from_string(normal_count_tag->value) / 3;
  geometry->normals = (Vector3*)sta_allocate(sizeof(Vector3) * normal_count);
  buffer            = sta_xml_create_buffer_from_content(normal_array);
  parse_vector3_array(&buffer, geometry->normals, normal_count);

  XML_Node* uv_source = normal_source->next;
  if (uv_source == 0)
  {
    printf("Failed to find normal source\n");
    return false;
  }
  XML_Node* uv_array = uv_source->find_key("float_array");
  if (uv_array == 0)
  {
    printf("Failed to find position array\n");
    return false;
  }
  XML_Tag* uv_count_tag = uv_array->find_tag("count");
  if (uv_count_tag == 0)
  {
    printf("Failed to find normal count\n");
    return false;
  }
  int uv_count = parse_int_from_string(uv_count_tag->value) / 2;
  geometry->uv = (Vector2*)sta_allocate(sizeof(Vector2) * uv_count);
  buffer       = sta_xml_create_buffer_from_content(uv_array);
  parse_vector2_array(&buffer, geometry->uv, uv_count);

  XML_Node* polylist = mesh->find_key("polylist");
  if (polylist == 0)
  {
    printf("Failed to find polylist\n");
    return false;
  }
  XML_Tag* p_count_tag = polylist->find_tag("count");
  if (p_count_tag == 0)
  {
    printf("Failed to find p_count_tag\n");
    return false;
  }

  buffer.index                  = 0;
  buffer.buffer                 = (char*)p_count_tag->value;
  buffer.len                    = p_count_tag->value_length;
  unsigned int       p_count    = parse_int_from_string(&buffer);

  const unsigned int face_count = 3;
  geometry->face_count          = p_count * face_count;
  geometry->faces               = (ColladaFaceIndexOrder*)sta_allocate(sizeof(ColladaFaceIndexOrder) * p_count * face_count);

  XML_Node* p                   = polylist->find_key("p");
  if (p == 0)
  {
    printf("Failed to find p\n");
    return false;
  }
  buffer = sta_xml_create_buffer_from_content(p);

  for (int i = 0; i < geometry->face_count; i++)
  {
    skip_whitespace(&buffer);
    geometry->faces[i].position_index = parse_int_from_string(&buffer);
    skip_whitespace(&buffer);
    geometry->faces[i].normal_index = parse_int_from_string(&buffer);
    skip_whitespace(&buffer);
    geometry->faces[i].uv_index = parse_int_from_string(&buffer);
  }

  return true;
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

void sta_collada_parse_controller_data(ColladaControllers* controllers, XML_Node* node)
{
  XML_Node* controller    = node->find_key("controller");
  XML_Node* skin          = controller->find_key("skin");
  XML_Node* weight_source = skin->child->next->next->next;
  XML_Node* weight_array  = weight_source->find_key("float_array");
  int       weight_count  = sta_xml_get_count_tag(weight_array);

  f32*      weights       = (f32*)sta_allocate(sizeof(f32) * weight_count);
  Buffer    buffer        = {};
  buffer                  = sta_xml_create_buffer_from_content(weight_array);
  parse_float_array(weights, weight_count, &buffer);

  XML_Node*    vertex_weights       = skin->find_key("vertex_weights");
  unsigned int vertex_weights_count = sta_xml_get_count_tag(vertex_weights);

  XML_Node*    vcount_node          = vertex_weights->find_key("vcount");
  i32*         vcount               = (i32*)sta_allocate(sizeof(i32) * vertex_weights_count);
  buffer                            = sta_xml_create_buffer_from_content(vcount_node);
  parse_int_array(vcount, vertex_weights_count, &buffer);

  XML_Node* v_node = vertex_weights->find_key("v");

  buffer           = sta_xml_create_buffer_from_content(v_node);

  printf("Parsing %d vertex weights count thingy\n", vertex_weights_count);
  controllers->vertex_weights_count = vertex_weights_count;
  controllers->joint_and_weight_idx = (ColladaJointAndWeightData*)sta_allocate(sizeof(ColladaJointAndWeightData) * controllers->vertex_weights_count);
  for (unsigned int i = 0; i < vertex_weights_count; i++)
  {
    i32 count                                  = vcount[i];

    controllers->joint_and_weight_idx[i].count = MIN(count, 4);
    for (int j = 0; j < count; j++)
    {
      skip_whitespace(&buffer);
      controllers->joint_and_weight_idx[i].joint_index[j] = parse_int_from_string(&buffer);
      skip_whitespace(&buffer);
      controllers->joint_and_weight_idx[i].weight[j] = weights[parse_int_from_string(&buffer)];
    }
    if (count > 4)
    {
      float w = 0.0f;
      for (int j = 0; j < 4; j++)
      {
        w += controllers->joint_and_weight_idx[i].weight[j];
      }
      for (int j = 0; j < 4; j++)
      {
        controllers->joint_and_weight_idx[i].weight[j] /= w;
      }
    }
  }

  XML_Node* joint_array    = skin->child->next->child;
  int       joint_count    = sta_xml_get_count_tag(joint_array);
  controllers->joints      = (Joint*)sta_allocate(sizeof(Joint) * joint_count);
  controllers->joint_count = joint_count;

  buffer                   = sta_xml_create_buffer_from_content(joint_array);
  for (int i = 0; i < joint_count; i++)
  {
    Joint* joint = &controllers->joints[i];
    skip_whitespace(&buffer);
    u64 index = buffer.index;
    while (buffer.buffer[buffer.index] != ' ' && buffer.buffer[buffer.index] != '\n' && buffer.buffer[buffer.index] != '<')
    {
      buffer.index++;
    }
    int name_length = buffer.index - index;
    joint->m_name   = (char*)sta_allocate(sizeof(char) * name_length);
    strncpy((char*)joint->m_name, &buffer.buffer[index], name_length);
    joint->m_name[name_length] = '\0';
  }

  XML_Node* matrix_array = skin->child->next->next->child;
  int       matrix_count = sta_xml_get_count_tag(matrix_array) / 16;
  buffer                 = sta_xml_create_buffer_from_content(matrix_array);
  for (int i = 0; i < matrix_count; i++)
  {
    Joint* joint = &controllers->joints[i];
    for (int i = 0; i < 16; i++)
    {
      skip_whitespace(&buffer);
      joint->m_invBindPose.m[i] = parse_float_from_string(&buffer);
    }
  }
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

static void animation_assign_parent(Skeleton* skeleton, XML_Node* curr, u8 p_idx)
{
  if (curr == 0)
  {
    return;
  }
  XML_Tag* tag                    = curr->find_tag("id");
  u8       idx                    = get_node_index_from_id(skeleton, tag->value, tag->value_length);
  skeleton->joints[idx].m_iParent = p_idx;
  XML_Node* matrix                = curr->find_key("matrix");

  float*    m                     = &skeleton->joints[idx].m_mat.m[0];
  Buffer    buffer                = sta_xml_create_buffer_from_content(matrix);
  parse_float_array(m, 16, &buffer);

  XML_Node* child = curr->find_key("node");
  if (child == 0)
  {
    return;
  }

  while (child)
  {
    animation_assign_parent(skeleton, child, idx);
    child = child->next;
    if (child)
    {
      child = child->find_key("node");
    }
  }
}

void sta_collada_parse_visual_scenes_data(AnimationModel* model, XML_Node* visual_scene)
{
  Skeleton* skeleton = &model->skeleton;
  XML_Node* node     = visual_scene->child->child;
  XML_Tag*  tag      = node->find_tag("id");
  XML_Node* child    = node->find_key("node");
  XML_Node* matrix   = node->find_key("matrix");
  float*    m        = &skeleton->joints[0].m_mat.m[0];
  Buffer    buffer   = sta_xml_create_buffer_from_content(matrix);
  parse_float_array(m, 16, &buffer);

  u8 idx = get_node_index_from_id(skeleton, tag->value, tag->value_length);
  while (child)
  {
    animation_assign_parent(skeleton, child, idx);
    child = child->next;
  }
}

void sta_collada_parse_animation_data(AnimationModel* model, XML_Node* library_animations)
{
  // animation is the array we iterate over
  XML_Node* animation          = library_animations->find_key("animation");
  model->animations.poses      = (JointPose*)sta_allocate(sizeof(JointPose) * model->skeleton.joint_count);
  model->animations.pose_count = 0;
  while (animation)
  {

    // find the channel and parse the name

    XML_Tag*   id_tag      = animation->find_tag("name");
    u8         idx         = get_node_index_from_id(&model->skeleton, id_tag->value, id_tag->value_length);

    JointPose* pose        = &model->animations.poses[idx];

    XML_Node*  input_array = animation->child->find_key("float_array");
    pose->name             = (char*)sta_allocate(sizeof(char) * id_tag->value_length);
    strncpy(pose->name, id_tag->value, id_tag->value_length);

    pose->steps           = sta_xml_get_count_tag(input_array);
    pose->t               = (f32*)sta_allocate(sizeof(f32) * pose->steps);
    pose->local_transform = (Mat44*)sta_allocate(sizeof(Mat44) * pose->steps);
    Buffer buffer         = sta_xml_create_buffer_from_content(input_array);
    parse_float_array(pose->t, pose->steps, &buffer);

    buffer = sta_xml_create_buffer_from_content(animation->child->next->child);

    for (unsigned int i = 0; i < pose->steps; i++)
    {
      for (int j = 0; j < 16; j++)
      {
        skip_whitespace(&buffer);
        float f                       = parse_float_from_string(&buffer);
        pose->local_transform[i].m[j] = f;
      }
    }

    animation = animation->next;
    model->animations.pose_count++;
  }
  printf("%d\n", model->animations.pose_count);
}

static void merge_geometry_data(AnimationModel* animation, ColladaControllers* controllers, ColladaGeometry* geometry)
{

  animation->vertex_count = geometry->face_count;
  animation->vertices     = (SkinnedVertex*)sta_allocate(sizeof(SkinnedVertex) * animation->vertex_count);
  animation->indices      = (u32*)sta_allocate(sizeof(u32) * animation->vertex_count);
  for (unsigned int face_index = 0; face_index < animation->vertex_count; face_index++)
  {
    animation->indices[face_index] = face_index;

    SkinnedVertex* vertex          = &animation->vertices[face_index];
    u32            position_index  = geometry->faces[face_index].position_index;
    u32            normal_index    = geometry->faces[face_index].normal_index;
    u32            uv_index        = geometry->faces[face_index].uv_index;

    vertex->position               = geometry->vertices[position_index];
    vertex->uv                     = geometry->uv[uv_index];
    vertex->normal                 = geometry->normals[normal_index];
    vertex->joint_index[0]         = controllers->joint_and_weight_idx[position_index].joint_index[0];
    vertex->joint_index[1]         = controllers->joint_and_weight_idx[position_index].joint_index[1];
    vertex->joint_index[2]         = controllers->joint_and_weight_idx[position_index].joint_index[2];
    vertex->joint_index[3]         = controllers->joint_and_weight_idx[position_index].joint_index[3];

    vertex->joint_weight[0]        = controllers->joint_and_weight_idx[position_index].weight[0];
    vertex->joint_weight[1]        = controllers->joint_and_weight_idx[position_index].weight[1];
    vertex->joint_weight[2]        = controllers->joint_and_weight_idx[position_index].weight[2];
    vertex->joint_weight[3]        = controllers->joint_and_weight_idx[position_index].weight[3];
  }

  animation->skeleton.joint_count = controllers->joint_count;
  animation->skeleton.joints      = controllers->joints;
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

  ColladaControllers controllers = {};
  ColladaGeometry    geometry    = {};

  // parse geometry
  XML_Node* geometry_node = xml.head.find_key("library_geometries");
  if (!geometry_node)
  {
    printf("Couldn't find %s\n", "library_geometries");
    return false;
  }
  sta_collada_parse_model_data(&geometry, geometry_node);

  XML_Node* controller = xml.head.find_key("library_controllers");
  sta_collada_parse_controller_data(&controllers, controller);

  merge_geometry_data(animation, &controllers, &geometry);

  XML_Node* visual_scene = xml.head.find_key("library_visual_scenes");
  sta_collada_parse_visual_scenes_data(animation, visual_scene);

  XML_Node* animations = xml.head.find_key("library_animations");
  sta_collada_parse_animation_data(animation, animations);

  return true;
}
