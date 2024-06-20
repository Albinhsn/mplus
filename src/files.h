#ifndef STA_FILES_H
#define STA_FILES_H
#include "common.h"
#include "string.h"
#include "vector.h"
#include <stdbool.h>
#include <stdio.h>
#define CURRENT_CHAR(buf) ((buf)->buffer[(buf)->index])
#define NEXT_CHAR(buf)    ((buf)->buffer[(buf)->index + 1])
#define ADVANCE(buffer)   ((buffer)->index++)
#define SKIP(buffer, cond)                                                                                                                                                                             \
  while (cond)                                                                                                                                                                                         \
  {                                                                                                                                                                                                    \
    ADVANCE(buffer);                                                                                                                                                                                   \
  }

struct Buffer
{
public:
  Buffer(char* b)
  {
    this->buffer = b;
    this->len    = 0;
    this->index  = 0;
  }
  Buffer()
  {
    this->buffer = 0;
    this->len    = 0;
    this->index  = 0;
  }
  Buffer(char* buffer, u64 len)
  {
    this->buffer = buffer;
    this->len    = len;
    this->index  = 0;
  }

  char* current_address()
  {
    return &this->buffer[this->index];
  }
  void* read(u64 size)
  {
    void* out = this->current_address();
    this->advance(size);
    return out;
  }
  void  parse_string_array(char** array, u64 count);
  void  parse_vector2_array(Vector2* array, u64 count);
  void  parse_vector3_array(Vector3* array, u64 count);
  float parse_float();
  void  parse_float_array(f32* array, u64 count);
  int   parse_int();
  void  parse_int_array(i32* array, u64 count);
  bool  match(char c);
  void  skip_whitespace();
  bool  is_out_of_bounds()
  {
    return this->len <= this->index;
  }
  void advance(int length)
  {
    this->index += length;
  }
  char advance()
  {
    return this->buffer[this->index++];
  }
  char current_char()
  {
    return this->buffer[this->index];
  }
  u64   len;
  u64   index;
  char* buffer;
};
struct VertexData
{
  Vector3 vertex;
  Vector2 uv;
  Vector3 normal;
};

struct ModelData
{

  VertexData* vertices;
  u32*        indices;
  u64         vertex_count;
};

struct TargaImage
{
  unsigned char* data;
  i32            bpp;
  u16            width, height;
};
typedef struct TargaImage TargaImage;
typedef struct TargaImage Image;

struct TargaHeader
{
  union
  {
    u8 header[18];
    struct
    {
      u8  charactersInIdentificationField;
      u8  colorMapType;
      u8  imageType;
      u8  colorMapSpec[5];
      u16 xOrigin;
      u16 yOrigin;
      u16 width;
      u16 height;
      u8  imagePixelSize;
      u8  imageDescriptor;
    };
  };
};
typedef struct TargaHeader TargaHeader;

struct CSVRecord
{
  Buffer* data;
  u64     dataCount;
  u64     dataCap;
};
typedef struct CSVRecord CSVRecord;

struct CSV
{
  CSVRecord* records;
  u64        recordCount;
  u64        recordCap;
};
typedef struct CSV CSV;

enum JsonType
{
  JSON_VALUE,
  JSON_OBJECT,
  JSON_ARRAY,
  JSON_STRING,
  JSON_NUMBER,
  JSON_BOOL,
  JSON_NULL
};
typedef enum JsonType JsonType;

struct JsonValue;
struct JsonObject;
struct JsonArray;

struct JsonValue
{
public:
  JsonType type;
  union
  {
    struct JsonObject* obj;
    struct JsonArray*  arr;
    bool               b;
    char*              string;
    float              number;
  };
};
typedef struct JsonValue JsonValue;

struct JsonObject
{
public:
  void debug_keys()
  {
    for (u32 i = 0; i < size; i++)
    {
      printf("%s\n", keys[i]);
    }
  }
  JsonValue* lookup_value(const char* key);
  char**     keys;
  JsonValue* values;
  u64        size;
  u64        cap;
};
typedef struct JsonObject JsonObject;

struct JsonArray
{
  uint32_t   arraySize;
  uint32_t   arrayCap;
  JsonValue* values;
};
typedef struct JsonArray JsonArray;

struct Json
{
  JsonType headType;
  union
  {
    JsonValue  value;
    JsonObject obj;
    JsonArray  array;
  };
};
typedef struct Json Json;

struct WavefrontVertexData
{
  u32 vertex_idx;
  u32 texture_idx;
  u32 normal_idx;
};

struct WavefrontFace
{
  WavefrontVertexData* vertices;
  u32                  count;
};

struct WavefrontObject
{
  u32            vertex_count;
  u32            vertex_capacity;
  Vector4*       vertices;

  Vector3*       texture_coordinates;
  u32            texture_coordinate_count;
  u32            texture_coordinate_capacity;

  u32            normal_count;
  u32            normal_capacity;
  Vector3*       normals;

  u32            face_count;
  u32            face_capacity;
  WavefrontFace* faces;
};

enum XML_TYPE
{
  XML_PARENT,
  XML_CONTENT,
  XML_CLOSED
};

struct XML_Tag
{
  char* name;
  u64   name_length;
  char* value;
  u64   value_length;
};
struct XML_Header
{
  char*    name;
  u64      name_length;
  XML_Tag* tags;
  u64      tag_count;
  u64      tag_capacity;
};

enum XML_Encoding
{
  UTF_8,
  UNKNOWN
};
struct XML_Node
{
public:
  XML_Node*  find_key_with_tag(char* key_name, u32 key_name_length, char* tag_name, u32 tag_name_length, char* tag_value, u32 tag_value_length);
  XML_Node*  find_key_with_tag(const char* key_name, const char* tag_name, String tag_value);
  XML_Node*  find_key_with_tag(const char* key_name, const char* tag_name, const char* tag_value);
  XML_Node*  find_key(const char* key_name);
  XML_Node*  find_key(char* key_name, u32 key_name_length);
  XML_Tag*   find_tag(const char* tag_name);
  XML_Tag*   find_tag(char* tag_name, u32 tag_name_length);
  XML_Node*  next;
  XML_Node*  parent;
  XML_TYPE   type;
  XML_Header header;
  union
  {
    XML_Node* child;
    String    content;
  };
};

struct XML
{
  XML_Node head;
  XML_Tag* version_and_encoding;
  u64      version_and_encoding_length;
  u64      version_and_encoding_capacity;
};

bool  sta_xml_parse_from_buffer(XML_Node* xml, Buffer* buffer);
u32   sta_xml_get_child_count_by_name(XML_Node* node, const char* name);
bool  sta_xml_parse_version_and_encoding(XML* xml, Buffer* buffer);
bool  remove_xml_key(XML_Node* xml, const char* node_name);
void  write_xml_to_file(XML* xml, const char* filename);

bool  sta_deserialize_json_from_string(Buffer* buffer, Json* json);
bool  sta_deserialize_json_from_file(Arena* arena, Json* json, const char* filename);
bool  sta_serialize_json_to_file(Json* json, const char* filename);
void  sta_json_debug(Json* json);
void  sta_json_debug_object(JsonObject* object);
void  sta_json_debug_array(JsonArray* arr);
void  sta_json_debug_value(JsonValue* value);

bool  sta_read_csv_from_file(Arena* arena, CSV* csv, const char* filelocation);
bool  sta_write_csv_to_file(CSV* csv, const char* filelocation);
void  sta_debug_csv(CSV* csv);
bool  sta_read_csv_from_string(CSV* csv, const char* csv_data);
void  sta_save_targa(TargaImage* image, const char* filename);

bool  sta_write_ppm(const char* filename, u8* data, u64 width, u64 height);
void  sta_draw_rect_to_image(u8* data, u64 image_width, u64 x, u64 y, u64 width, u64 height, u8 r, u8 g, u8 b, u8 a);
bool  sta_read_targa_from_file_rgba(TargaImage* image, const char* filename);
bool  sta_read_targa_from_file(Arena* arena, TargaImage* image, const char* filename);
bool  sta_read_file(Arena* arena, Buffer* string, const char* fileName);
bool  sta_read_file(Buffer* buffer, const char* fileName);
bool  sta_append_to_file(const char* filename, const char* message);

bool  sta_parse_wavefront_object_from_file(ModelData* model, const char* filename);
bool  sta_convert_obj_to_model(const char* input_filename, const char* output_filename);

int   parse_int_from_string(Buffer* buffer);
float parse_float_from_string(Buffer* buffer);
void  skip_whitespace(Buffer* buffer);
bool  match(Buffer* buffer, char target);

#endif
