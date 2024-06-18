#include "gltf.h"
#include "common.h"
#include "files.h"
#include "platform.h"
#include <cassert>

#define GLTF_CHUNK_TYPE_JSON 0x4E4F534A
#define GLTF_CHUNK_TYPE_BIN  0x004E4942

struct GLTF_Header
{
  u32 magic;
  u32 version;
  u32 length;
};

struct GLTF_Chunk
{
  u32            chunk_length;
  u32            chunk_type;
  unsigned char* chunk_data;
};

struct GLTF_Node
{
  f32*  children;
  f32*  children_count;
  char* name;
  f32   rotation[4];
  f32   scale[3];
  f32   translation[3];
};

static inline void read_chunk(Buffer* buffer, GLTF_Chunk* chunk)
{

  chunk->chunk_length = *(u32*)buffer->read(sizeof(u32));
  chunk->chunk_type   = *(u32*)buffer->read(sizeof(u32));
  chunk->chunk_data   = (unsigned char*)buffer->read(sizeof(unsigned char) * chunk->chunk_length);
}

bool parse_gltf(const char* filename)
{

  Buffer buffer = {};
  sta_read_file(&buffer, filename);
  GLTF_Header header = *(GLTF_Header*)buffer.read(sizeof(GLTF_Header));
  printf("Magic: %x, version: %d, length: %d\n", header.magic, header.version, header.length);

  GLTF_Chunk chunk0 = {};
  GLTF_Chunk chunk1 = {};
  read_chunk(&buffer, &chunk0);
  read_chunk(&buffer, &chunk1);

  Json   json_data = {};
  Arena  arena(4096 * 4096);
  Buffer buffer_chunk0 = {};
  buffer_chunk0.buffer = (char*)chunk0.chunk_data;
  buffer_chunk0.len    = chunk0.chunk_length;
  bool result          = sta_deserialize_json_from_string(&buffer_chunk0, &arena, &json_data);
  if (!result)
  {
    printf("Failed to deserialize json!\n");
    return false;
  }
  // sta_json_debug(&json_data);
  // sta_serialize_json_to_file(&json_data, "test.json");

  JsonObject* head = &json_data.obj;
  for (u32 i = 0; i < head->size; i++)
  {
    printf("%s\n", head->keys[i]);
  }

  JsonValue* json_nodes = head->lookup_value("nodes");
  sta_json_debug_value(json_nodes);
  assert(json_nodes->type == JSON_ARRAY && "Nodes should be an array?");

  GLTF_Node* nodes = (GLTF_Node*)sta_allocate_struct(GLTF_Node, json_nodes->arr->arraySize);
  for (u32 i = 0; i < json_nodes->arr->arraySize; i++)
  {
    JsonObject* node_object = json_nodes->arr->values[i].obj;
    GLTF_Node*  node        = &nodes[i];
    node->name              = node_object->lookup_value("name")->string;
  }

  JsonValue* scenes      = head->lookup_value("scenes");
  JsonValue* animations  = head->lookup_value("animations");
  JsonValue* meshes      = head->lookup_value("meshes");
  JsonValue* skins       = head->lookup_value("skins");
  JsonValue* buffers     = head->lookup_value("buffers");
  JsonValue* bufferViews = head->lookup_value("bufferViews");

  return true;
}

#undef GLTF_CHUNK_TYPE_JSON
