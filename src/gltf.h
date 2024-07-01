#ifndef GLTF_H
#define GLTF_H

#include "animation.h"
#include "common.h"
#include "files.h"

#define GLTF_CHUNK_TYPE_JSON 0x4E4F534A
#define GLTF_CHUNK_TYPE_BIN  0x004E4942

enum GLTF_ChannelTargetPath
{
  CHANNEL_TARGET_PATH_TRANSLATION,
  CHANNEL_TARGET_PATH_SCALE,
  CHANNEL_TARGET_PATH_ROTATION
};

enum GLTF_ComponentType
{
  COMPONENT_TYPE_BYTE           = 5120,
  COMPONENT_TYPE_UNSIGNED_BYTE  = 5121,
  COMPONENT_TYPE_SHORT          = 5122,
  COMPONENT_TYPE_UNSIGNED_SHORT = 5123,
  COMPONENT_TYPE_UNSIGNED_INT   = 5125,
  COMPONENT_TYPE_FLOAT          = 5126
};

enum GLTF_AccessorType
{
  ACCESSOR_TYPE_VEC2,
  ACCESSOR_TYPE_VEC3,
  ACCESSOR_TYPE_VEC4,
  ACCESSOR_TYPE_SCALAR,
  ACCESSOR_TYPE_MAT4
};

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

struct GLTF_BufferView
{
  u8* buffer;
  u32 buffer_length;
};

struct GLTF_Accessor
{
  u32                buffer_view_index;
  GLTF_ComponentType component_type;
  u32                count;
  GLTF_AccessorType  type;
};

bool gltf_parse(AnimationModel * model, const char * filename);

#endif
