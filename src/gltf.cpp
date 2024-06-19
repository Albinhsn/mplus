#include "gltf.h"
#include "animation.h"
#include "common.h"
#include "files.h"
#include "platform.h"
#include <cassert>
#include <cstring>

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
public:
  void debug()
  {
    printf("%s:\n", name);
    printf("translation: %f %f %f\n", translation[0], translation[1], translation[2]);
    printf("rotation : %f %f %f %f\n", rotation[0], rotation[1], rotation[2], rotation[3]);
    printf("scale: %f %f %f\n", scale[0], scale[1], scale[2]);
    printf("Children: [");
    for (u32 i = 0; i < children_count; i++)
    {
      printf("%f", children[i]);
      if (i < children_count - 1)
      {
        printf(",");
      }
    }
    printf("]\n");
  }
  f32*  children;
  u32   children_count;
  char* name;
  // builds a local transform in column major
  // M = T * R * S
  f32 rotation[4];
  f32 scale[3];
  f32 translation[3];
};

static inline void read_chunk(Buffer* buffer, GLTF_Chunk* chunk)
{

  chunk->chunk_length = *(u32*)buffer->read(sizeof(u32));
  chunk->chunk_type   = *(u32*)buffer->read(sizeof(u32));
  chunk->chunk_data   = (unsigned char*)buffer->read(sizeof(unsigned char) * chunk->chunk_length);
}

void parse_gltf_node(GLTF_Node* node, JsonObject* object)
{
  memset(node, 0, sizeof(GLTF_Node));
  node->name          = object->lookup_value("name")->string;

  JsonValue* rotation = object->lookup_value("rotation");
  if (rotation)
  {
    assert(rotation->type == JSON_ARRAY && rotation->arr->arraySize == 4 && "Thought rotation was array and quaternion?");
    for (u32 i = 0; i < 4; i++)
    {
      assert(rotation->arr->values[i].type == JSON_NUMBER && "Rotation value wasn't number?");
      node->rotation[i] = rotation->arr->values[i].number;
    }
  }
  JsonValue* scale = object->lookup_value("scale");
  if (scale)
  {
    assert(scale->type == JSON_ARRAY && scale->arr->arraySize == 3 && "Thought scale was array and quaternion?");
    for (u32 i = 0; i < 3; i++)
    {
      assert(scale->arr->values[i].type == JSON_NUMBER && "Scale value wasn't number?");
      node->scale[i] = scale->arr->values[i].number;
    }
  }

  JsonValue* translation = object->lookup_value("translation");
  if (translation)
  {
    assert(translation->type == JSON_ARRAY && translation->arr->arraySize == 3 && "Thought translation was array and quaternion?");
    for (u32 i = 0; i < 3; i++)
    {
      assert(translation->arr->values[i].type == JSON_NUMBER && "translation value wasn't number?");
      node->translation[i] = translation->arr->values[i].number;
    }
  }
  JsonValue* children = object->lookup_value("children");
  if (children)
  {
    assert(children->type == JSON_ARRAY && "Children isn't an array?");
    node->children_count = children->arr->arraySize;
    node->children       = (f32*)sta_allocate_struct(f32, node->children_count);
    for (u32 i = 0; i < node->children_count; i++)
    {
      assert(children->arr->values[i].type == JSON_NUMBER && "Child wasn't a number?");
      node->children[i] = children->arr->values[i].number;
    }
  }
}

bool parse_gltf(AnimationModel* model, const char* filename)
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

  JsonObject* head       = &json_data.obj;
  JsonValue*  json_nodes = head->lookup_value("nodes");
  assert(json_nodes->type == JSON_ARRAY && "Nodes should be an array?");

  GLTF_Node* nodes = (GLTF_Node*)sta_allocate_struct(GLTF_Node, json_nodes->arr->arraySize);
  u32        count = 0;
  for (u32 i = 0; i < json_nodes->arr->arraySize; i++)
  {
    JsonObject* node_object = json_nodes->arr->values[i].obj;
    char*       name        = node_object->lookup_value("name")->string;
    if (strlen(name) == strlen("Armature") && strncmp(name, "Armature", strlen("Armature")) == 0)
    {
      continue;
    }
    if (node_object->lookup_value("skin"))
    {
      continue;
    }
    ++count;
    parse_gltf_node(&nodes[i], node_object);
  }

  Skeleton* skeleton    = &model->skeleton;
  skeleton->joint_count = count;
  skeleton->joints      = (Joint*)sta_allocate_struct(Joint, count);
  for (u32 i = 0; i < count; i++)
  {
    Joint* joint  = &skeleton->joints[count - i];
    joint->m_name = nodes[i].name;
    for (u32 j = 0; j < nodes[i].children_count; j++)
    {
      skeleton->joints[(i32)(count - nodes[i].children[j])].m_iParent = count - i;
    }
  }

  JsonValue* scenes      = head->lookup_value("scenes");
  JsonValue* animations  = head->lookup_value("animations");
  JsonValue* meshes      = head->lookup_value("meshes");
  JsonValue* skins       = head->lookup_value("skins");
  JsonValue* buffers     = head->lookup_value("buffers");
  JsonValue* bufferViews = head->lookup_value("bufferViews");
  JsonValue* accessors   = head->lookup_value("accessors");

  return true;
}

#undef GLTF_CHUNK_TYPE_JSON
