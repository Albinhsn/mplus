#include "animation.h"
#include "files.h"
#include "platform.h"
#include <cctype>
#include <cstring>

#define CURRENT_CHAR(buf) ((buf)->buffer[(buf)->index])
#define NEXT_CHAR(buf)    ((buf)->buffer[(buf)->index + 1])
#define ADVANCE(buffer)   ((buffer)->index++)
#define SKIP(buffer, cond)                                                                                                                                                                             \
  while (cond)                                                                                                                                                                                         \
  {                                                                                                                                                                                                    \
    ADVANCE(buffer);                                                                                                                                                                                   \
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
XML_Node* find_xml_key(XML_Node* xml, const char* node_name, u64 node_name_length)
{
  XML_Node* curr = xml->child;
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

bool match(Buffer* buffer, char target)
{
  return CURRENT_CHAR(buffer) == target;
}

void skip_whitespace(Buffer* buffer)
{
  SKIP(buffer, match(buffer, ' ') || match(buffer, '\n'));
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

bool sta_parse_collada_file(XML* xml, const char* filename)
{

  Buffer buffer = {};
  if (!sta_read_file(&buffer, filename))
  {
    return 0;
  }

  if (!parse_version_and_encoding(xml, &buffer))
  {
    return false;
  }

  return parse_xml(&xml->head, &buffer);
}
