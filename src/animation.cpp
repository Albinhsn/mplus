#include "animation.h"
#include "files.h"
#include "platform.h"
#include <cctype>
#include <cstring>

#define CURRENT_CHAR(buf) ((buf)->buffer[(buf)->index])
#define NEXT_CHAR(buf)    ((buf)->buffer[(buf)->index + 1])
#define ADVANCE(buffer)   ((buffer)->index++)

void debug_tag(XML_Tag* tag)
{
  printf("%.*s=\"%.*s\" ", (i32)tag->name_length, tag->name, (i32)tag->value_length, tag->value);
}

void debug_xml(XML* xml)
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
    debug_xml(xml->child);
  }
  else if (xml->content_length != 0)
  {
    printf("%.*s\n", (i32)xml->content_length, xml->content);
  }
  printf("</%.*s>\n", (i32)header->name_length, header->name);
  debug_xml(xml->next);
}

void skip_whitespace(Buffer* buffer)
{
  while (CURRENT_CHAR(buffer) == ' ' || CURRENT_CHAR(buffer) == '\n')
  {
    ADVANCE(buffer);
  }
}

static bool compare_string(XML_Header* xml, Buffer* buffer, u64 index)
{
  u64 len = buffer->index - index;
  if (len != xml->name_length)
  {
    printf("Not the same length! %ld %ld %ld %ld %.*s\n", len, xml->name_length, buffer->index, index, (i32)len, &buffer->buffer[index]);
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
XML_Header_Result parse_header(XML* xml, Buffer* buffer)
{
  const int initial_tag_capacity = 8;
  // <NAME [TAG_NAME="TAG_CONTENT"]>
  XML_Header* header   = &xml->header;
  header->name         = &CURRENT_CHAR(buffer);
  header->tag_capacity = initial_tag_capacity;
  header->tags         = (XML_Tag*)allocate(sizeof(XML_Tag) * initial_tag_capacity);
  u64 index            = buffer->index;
  while (CURRENT_CHAR(buffer) != ' ' && CURRENT_CHAR(buffer) != '>' && CURRENT_CHAR(buffer) != '/')
  {
    ADVANCE(buffer);
  }
  header->name_length = buffer->index - index;

  // parse tags
  for (;;)
  {
    skip_whitespace(buffer);
    if (isalpha(CURRENT_CHAR(buffer)))
    {
      RESIZE_ARRAY(header->tags, XML_Tag, header->tag_count, header->tag_capacity);
      XML_Tag* current_tag = &header->tags[header->tag_count++];

      // parse name
      u64 index         = buffer->index;
      current_tag->name = &CURRENT_CHAR(buffer);

      // very robust yes
      while (CURRENT_CHAR(buffer) != '=')
      {
        ADVANCE(buffer);
      }
      current_tag->name_length = buffer->index - index;

      ADVANCE(buffer);
      if (CURRENT_CHAR(buffer) != '\"')
      {
        printf("Expected '\"' for tag content?\n");
        return XML_HEADER_ERROR;
      }
      ADVANCE(buffer);
      current_tag->value = &CURRENT_CHAR(buffer);
      index              = buffer->index;

      while (CURRENT_CHAR(buffer) != '\"')
      {
        ADVANCE(buffer);
      }

      current_tag->value_length = buffer->index - index;
      ADVANCE(buffer);
    }
    else if (CURRENT_CHAR(buffer) == '/' && NEXT_CHAR(buffer) == '>')
    {
      ADVANCE(buffer);
      ADVANCE(buffer);
      return XML_HEADER_CLOSED;
    }
    else if (CURRENT_CHAR(buffer) == '>')
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

bool close_xml(XML* xml, Buffer* buffer)
{

  ADVANCE(buffer);
  u64 index = buffer->index;
  while (CURRENT_CHAR(buffer) != '>')
  {
    ADVANCE(buffer);
  }
  if (!compare_string(&xml->header, buffer, index))
  {
    printf("Closed the wrong thing?\n");
    return false;
  }
  ADVANCE(buffer);
  return true;
}

bool parse_content(XML* xml, Buffer* buffer, u64 index)
{
  xml->type    = XML_CONTENT;
  xml->content = &CURRENT_CHAR(buffer);
  while (CURRENT_CHAR(buffer) != '<')
  {
    ADVANCE(buffer);
  }
  xml->content_length = buffer->index - index;
  ADVANCE(buffer);
  if (CURRENT_CHAR(buffer) != '/')
  {
    printf("Should be closing?\n");
    return false;
  }
  return close_xml(xml, buffer);
}

bool parse_xml(XML* xml, Buffer* buffer)
{
  // parse first thing?

  skip_whitespace(buffer);
  if (CURRENT_CHAR(buffer) != '<')
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
    return true;
  }

  skip_whitespace(buffer);
  u64 index = buffer->index;
  if (CURRENT_CHAR(buffer) == '<')
  {
    // check empty
    if (NEXT_CHAR(buffer) == '/')
    {
      // advance past '<'
      ADVANCE(buffer);
      return close_xml(xml, buffer);
    }
    xml->type          = XML_PARENT;
    xml->child         = (XML*)allocate(sizeof(XML));
    xml->child->parent = xml;
    if (!parse_xml(xml->child, buffer))
    {
      return false;
    }

    printf("Next? %c\n", CURRENT_CHAR(buffer));
    XML* next = xml->child;
    for (;;)
    {
      skip_whitespace(buffer);
      if (CURRENT_CHAR(buffer) != '<')
      {
        return true;
      }
      if(NEXT_CHAR(buffer) == '/'){
        ADVANCE(buffer);
        return close_xml(xml, buffer);
      }
      next->next         = (XML*)allocate(sizeof(XML));
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

bool sta_parse_collada_file(XML* xml, const char* filename)
{

  Buffer buffer = {};
  if (!sta_read_file(&buffer, filename))
  {
    return 0;
  }

  return parse_xml(xml, &buffer);
}
