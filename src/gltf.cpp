#include "gltf.h"
#include "common.h"
#include "files.h"

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
  sta_json_debug(&json_data);
  return true;
}

#undef GLTF_CHUNK_TYPE_JSON
