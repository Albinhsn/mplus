#include "animation.h"
#include "files.h"
#include "platform.h"
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>

#define CURRENT_CHAR(buf) ((buf)->buffer[(buf)->index])
#define NEXT_CHAR(buf)    ((buf)->buffer[(buf)->index + 1])
#define ADVANCE(buffer)   ((buffer)->index++)
#define SKIP(buffer, cond)                                                                                                                                                                             \
  while (cond)                                                                                                                                                                                         \
  {                                                                                                                                                                                                    \
    ADVANCE(buffer);                                                                                                                                                                                   \
  }

float parse_float_from_string(Buffer* buffer)
{
  bool sign = false;
  if (CURRENT_CHAR(buffer) == '-')
  {
    ADVANCE(buffer);
    sign = true;
  }
  float value = 0;
  while (isdigit(CURRENT_CHAR(buffer)))
  {
    value *= 10;
    value += CURRENT_CHAR(buffer) - '0';
    ADVANCE(buffer);
  }

  float decimal       = 0.0f;
  int   decimal_count = 0;
  if (CURRENT_CHAR(buffer) == '.')
  {
    ADVANCE(buffer);
    while (isdigit(CURRENT_CHAR(buffer)))
    {
      decimal_count++;
      decimal += (CURRENT_CHAR(buffer) - '0') / pow(10, decimal_count);
      ADVANCE(buffer);
    }
  }
  value += decimal;

  return sign ? -value : value;
}
int parse_int_from_string(Buffer* buffer)
{
  bool sign = false;
  if (CURRENT_CHAR(buffer) == '-')
  {
    ADVANCE(buffer);
    sign = true;
  }
  u64 value = 0;
  while (isdigit(CURRENT_CHAR(buffer)))
  {
    value *= 10;
    value += CURRENT_CHAR(buffer) - '0';
    ADVANCE(buffer);
  }
  return sign ? -value : value;
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

  deallocate(node->header.tags, sizeof(XML_Tag) * node->header.tag_capacity);
  deallocate(node, sizeof(XML_Node));
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
XML_Node* sta_xml_find_key(XML_Node* xml, const char* node_name)
{
  u64       node_name_length = strlen(node_name);
  XML_Node* curr             = xml->child;
  while (curr != 0)
  {
    if (curr->header.name_length == node_name_length && strncmp(curr->header.name, node_name, node_name_length) == 0)
    {
      return curr;
    }
    curr = curr->next;
  }
  return 0;
}

XML_Tag* sta_xml_find_tag(XML_Node* xml, const char* tag_name)
{
  XML_Tag*           tags            = xml->header.tags;
  const unsigned int tag_name_length = strlen(tag_name);
  for (unsigned int i = 0; i < xml->header.tag_count; i++)
  {
    XML_Tag* tag = &tags[i];
    if (tag->name_length == tag_name_length && strncmp(tag_name, tag->name, tag_name_length) == 0)
    {
      return tag;
    }
  }
  return 0;
}

bool match(Buffer* buffer, char target)
{
  return CURRENT_CHAR(buffer) == target;
}

void skip_whitespace(Buffer* buffer)
{
  SKIP(buffer, match(buffer, ' ') || match(buffer, '\n') || match(buffer, '\t'));
}

void debug_tag(XML_Tag* tag)
{
  printf("%.*s=\"%.*s\" ", (i32)tag->name_length, tag->name, (i32)tag->value_length, tag->value);
}

void debug_xml_node(XML_Node* xml)
{
  if (xml == 0)
  {
    return;
  }
  XML_Header* header = &xml->header;
  printf("<%.*s ", (i32)header->name_length, header->name);
  for (unsigned int i = 0; i < header->tag_count; i++)
  {
    debug_tag(&header->tags[i]);
  }
  printf(">\n");
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
  header->tags         = (XML_Tag*)allocate(sizeof(XML_Tag) * initial_tag_capacity);
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
    xml->child         = (XML_Node*)allocate(sizeof(XML_Node));
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
      next->next         = (XML_Node*)allocate(sizeof(XML_Node));
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
  xml->version_and_encoding          = (XML_Tag*)allocate(sizeof(XML_Tag) * initial_tag_capacity);
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

bool sta_collada_parse_model_data(ModelData* model, XML_Node* node)
{
  XML_Node* geometry = sta_xml_find_key(node, "geometry");
  if (geometry == 0)
  {
    printf("Failed to find geometry\n");
    return false;
  }
  XML_Node* mesh = sta_xml_find_key(geometry, "mesh");
  if (mesh == 0)
  {
    printf("Failed to find mesh\n");
    return false;
  }
  XML_Node* position_source = sta_xml_find_key(mesh, "source");
  if (position_source == 0)
  {
    printf("Failed to find position source\n");
    return false;
  }
  XML_Node* position_array = sta_xml_find_key(position_source, "float_array");
  if (position_array == 0)
  {
    printf("Failed to find position array\n");
    return false;
  }
  XML_Tag* position_count_tag = sta_xml_find_tag(position_array, "count");
  if (position_count_tag == 0)
  {
    printf("Failed to find count\n");
    return false;
  }

  int      position_count = parse_int_from_string(position_count_tag->value) / 3;
  Vector3* positions      = (Vector3*)allocate(sizeof(Vector3) * position_count);
  Buffer   buffer         = {};
  buffer.buffer           = (char*)position_array->content;
  buffer.len              = position_array->content_length;
  parse_vector3_array(&buffer, positions, position_count);

  XML_Node* normal_source = position_source->next;
  if (normal_source == 0)
  {
    printf("Failed to find normal source\n");
    return false;
  }
  XML_Node* normal_array = sta_xml_find_key(normal_source, "float_array");
  if (normal_array == 0)
  {
    printf("Failed to find position array\n");
    return false;
  }
  XML_Tag* normal_count_tag = sta_xml_find_tag(normal_array, "count");
  if (normal_count_tag == 0)
  {
    printf("Failed to find normal count\n");
    return false;
  }
  int      normal_count = parse_int_from_string(normal_count_tag->value) / 3;
  Vector3* normals      = (Vector3*)allocate(sizeof(Vector3) * normal_count);
  buffer.index          = 0;
  buffer.buffer         = (char*)normal_array->content;
  buffer.len            = normal_array->content_length;
  parse_vector3_array(&buffer, normals, normal_count);

  XML_Node* uv_source = normal_source->next;
  if (uv_source == 0)
  {
    printf("Failed to find normal source\n");
    return false;
  }
  XML_Node* uv_array = sta_xml_find_key(uv_source, "float_array");
  if (uv_array == 0)
  {
    printf("Failed to find position array\n");
    return false;
  }
  XML_Tag* uv_count_tag = sta_xml_find_tag(uv_array, "count");
  if (uv_count_tag == 0)
  {
    printf("Failed to find normal count\n");
    return false;
  }
  int      uv_count = parse_int_from_string(uv_count_tag->value) / 2;
  Vector2* uvs      = (Vector2*)allocate(sizeof(Vector2) * uv_count);
  buffer.index      = 0;
  buffer.buffer     = (char*)uv_array->content;
  buffer.len        = uv_array->content_length;
  parse_vector2_array(&buffer, uvs, uv_count);

  XML_Node* polylist = sta_xml_find_key(mesh, "polylist");
  if (polylist == 0)
  {
    printf("Failed to find polylist\n");
    return false;
  }
  XML_Tag* p_count_tag = sta_xml_find_tag(polylist, "count");
  if (p_count_tag == 0)
  {
    printf("Failed to find p_count_tag\n");
    return false;
  }

  buffer.index         = 0;
  buffer.buffer        = (char*)p_count_tag->value;
  buffer.len           = p_count_tag->value_length;
  unsigned int p_count = parse_int_from_string(&buffer);

  const unsigned int face_count = 3;
  model->vertex_count = p_count * face_count;
  model->vertices     = (VertexData*)allocate(sizeof(VertexData) * p_count * face_count);
  model->indices      = (u32*)allocate(sizeof(u32) * p_count * face_count);
  float     low = FLT_MAX, high = -FLT_MAX;

  XML_Node* p = sta_xml_find_key(polylist, "p");
  if (p == 0)
  {
    printf("Failed to find p\n");
    return false;
  }
  buffer.buffer = (char*)p->content;
  buffer.len    = p->content_length;
  buffer.index  = 0;

  for (unsigned int i = 0; i < p_count; i++)
  {
    for (unsigned int j = 0; j < face_count; j++)
    {
      u64         index  = i * face_count + j;
      VertexData* vertex = &model->vertices[index];
      skip_whitespace(&buffer);
      u64 position_index = parse_int_from_string(&buffer);
      skip_whitespace(&buffer);
      u64 normals_index = parse_int_from_string(&buffer);
      skip_whitespace(&buffer);
      u64 uv_index = parse_int_from_string(&buffer);

      Vector3 v = positions[position_index];
      if (low > v.x)
      {
        low = v.x;
      }
      if (low > v.y)
      {
        low = v.y;
      }
      if (low > v.z)
      {
        low = v.z;
      }
      if (high < v.x)
      {
        high = v.x;
      }
      if (high < v.y)
      {
        high = v.y;
      }
      if (high < v.z)
      {
        high = v.z;
      }

      model->indices[index] = index;
      vertex->vertex        = v;
      vertex->normal        = normals[normals_index];
      vertex->uv            = uvs[uv_index];
    }
  }

  float diff = high - low;
  for (unsigned int i = 0; i < model->vertex_count; i++)
  {
    VertexData* vertex = &model->vertices[i];
    Vector3*    v      = &vertex->vertex;
    if (low < 0)
    {
      v->x = ((v->x - low) / diff) * 2.0f - 1.0f;
      v->y = ((v->y - low) / diff) * 2.0f - 1.0f;
      v->z = ((v->z - low) / diff) * 2.0f - 1.0f;
    }
    else
    {
      v->x = ((v->x + low) / diff) * 2.0f - 1.0f;
      v->y = ((v->y + low) / diff) * 2.0f - 1.0f;
      v->z = ((v->z + low) / diff) * 2.0f - 1.0f;
    }
  }

  return true;
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

  // parse geometry
  XML_Node* geometry = sta_xml_find_key(&xml.head, "library_geometries");
  if (!geometry)
  {
    printf("Couldn't find %s\n", "library_geometries");
    return false;
  }
  sta_collada_parse_model_data(&animation->model_data, geometry);

  // parse animations
  // parse controls
  XML_Node* animations = sta_xml_find_key(&xml.head, "library_animations");
  if (!animations)
  {
    printf("Couldn't find %s\n", "library_animations");
    return false;
  }
  return true;
}
