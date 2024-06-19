#include "gltf.h"
#include "animation.h"
#include "common.h"
#include "files.h"
#include "platform.h"
#include <cassert>
#include <cstdlib>
#include <cstring>

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

static inline void read_chunk(Buffer* buffer, GLTF_Chunk* chunk)
{

  chunk->chunk_length = *(u32*)buffer->read(sizeof(u32));
  chunk->chunk_type   = *(u32*)buffer->read(sizeof(u32));
  chunk->chunk_data   = (unsigned char*)buffer->read(sizeof(unsigned char) * chunk->chunk_length);
}

static inline void get_buffer_view_buffer(GLTF_Chunk* chunk, u8** buffer, JsonObject* view)
{
  *buffer = &chunk->chunk_data[(u32)view->lookup_value("byteOffset")->number];
}

static void parse_buffer_views(GLTF_Chunk* binary_chunk, GLTF_BufferView** views, u32& count, JsonValue* json_buffer_views)
{
  JsonArray*       buffer_views_array = json_buffer_views->arr;
  GLTF_BufferView* buffer_views       = (GLTF_BufferView*)sta_allocate_struct(GLTF_BufferView, buffer_views_array->arraySize);
  for (u32 i = 0; i < buffer_views_array->arraySize; i++)
  {
    JsonValue* buffer_view_value = &buffer_views_array->values[i];
    get_buffer_view_buffer(binary_chunk, &buffer_views[i].buffer, buffer_view_value->obj);
    buffer_views[i].buffer_length = buffer_view_value->obj->lookup_value("byteLength")->number;
  }

  *views = buffer_views;
}

static void parse_accessor_type(GLTF_AccessorType* type, char* type_str)
{
  if (strlen(type_str) == 4 && strncmp(type_str, "VEC4", 4) == 0)
  {
    *type = ACCESSOR_TYPE_VEC4;
  }
  else if (strlen(type_str) == 4 && strncmp(type_str, "VEC3", 4) == 0)
  {
    *type = ACCESSOR_TYPE_VEC3;
  }
  else if (strlen(type_str) == 4 && strncmp(type_str, "VEC2", 4) == 0)
  {
    *type = ACCESSOR_TYPE_VEC2;
  }
  else if (strlen(type_str) == 4 && strncmp(type_str, "MAT4", 4) == 0)
  {
    *type = ACCESSOR_TYPE_MAT4;
  }
  else if (strlen(type_str) == 6 && strncmp(type_str, "SCALAR", 6) == 0)
  {
    *type = ACCESSOR_TYPE_SCALAR;
  }
  else
  {
    assert(!"Don't know how to get this accessor type?");
  }
}

static void parse_accessors(GLTF_Accessor** accessors, u32& count, JsonValue* accessors_json)
{
  JsonArray*     accessors_array = accessors_json->arr;
  GLTF_Accessor* acc             = (GLTF_Accessor*)sta_allocate_struct(GLTF_Accessor, accessors_array->arraySize);
  count                          = accessors_array->arraySize;
  for (u32 i = 0; i < accessors_array->arraySize; i++)
  {
    JsonObject*    accessor_obj = accessors_array->values[i].obj;
    GLTF_Accessor* accessor     = &acc[i];

    accessor->buffer_view_index = accessor_obj->lookup_value("bufferView")->number;
    accessor->type              = (GLTF_AccessorType)accessor_obj->lookup_value("componentType")->number;
    accessor->count             = accessor_obj->lookup_value("count")->number;
    parse_accessor_type(&accessor->type, accessor_obj->lookup_value("type")->string);
  }

  *accessors = acc;
}

static inline GLTF_ChannelTargetPath get_channel_target_path(char* path)
{
  if (strlen(path) == 5 && strncmp(path, "scale", 5) == 0)
  {
    return CHANNEL_TARGET_PATH_SCALE;
  }
  else if (strlen(path) == strlen("rotation") && strncmp(path, "rotation", strlen("rotation")) == 0)
  {
    return CHANNEL_TARGET_PATH_ROTATION;
  }
  else if (strlen(path) == strlen("translation") && strncmp(path, "translation", strlen("translation")) == 0)
  {
    return CHANNEL_TARGET_PATH_TRANSLATION;
  }
  assert(!"Unknown channel path!");
}

static inline bool compare_float(f32 a, f32 b)
{
  const float EPSILON = 0.0001f;
  return std::abs(a - b) < EPSILON;
}
struct LL
{
  LL* next;
  LL* prev;
  f32 time;
};
struct AnimationData
{
  Mat44* T;
  f32*   T_steps;
  u32    T_count;
  Mat44* R;
  f32*   R_steps;
  u32    R_count;
  Mat44* S;
  f32*   S_steps;
  u32    S_count;
};

static f32 calculate_steps(AnimationData* data, LL* steps, u32& step_count)
{

  step_count       = data->T_count;
  f32 max_duration = 0.0f;
  for (u32 j = 0; j < data->T_count; j++)
  {
    steps[j].time = data->T_steps[j];
    if (j != 0)
    {
      steps[j].prev = &steps[j - 1];
    }
    if (j != data->T_count - 1)
    {
      steps[j].next = &steps[j + 1];
    }
    max_duration = MAX(max_duration, steps[j].time);
  }

  for (u32 j = 0, step_index = 0; j < data->R_count;)
  {
    if (compare_float(data->R_steps[j], steps[step_index].time))
    {
      step_index++;
      j++;
    }
    else if (data->R_steps[j] < steps[step_index].time)
    {
      LL* new_step                 = &steps[step_count++];
      new_step->next               = &steps[step_index];
      new_step->prev               = steps[step_index].prev;
      steps[step_index].prev->next = new_step;
      steps[step_index].prev       = new_step;
      new_step->time               = data->R_steps[j];
      j++;
    }
    else
    {
      step_index++;
    }
    max_duration = MAX(max_duration, data->R_steps[j]);
  }

  for (u32 j = 0, step_index = 0; j < data->S_count;)
  {
    if (compare_float(data->S_steps[j], steps[step_index].time))
    {
      step_index++;
      j++;
    }
    else if (data->S_steps[j] < steps[step_index].time)
    {
      LL* new_step                 = &steps[step_count++];
      new_step->next               = &steps[step_index];
      new_step->prev               = steps[step_index].prev;
      steps[step_index].prev->next = new_step;
      steps[step_index].prev       = new_step;
      new_step->time               = data->S_steps[j];
      j++;
    }
    else
    {
      step_index++;
    }
    max_duration = MAX(max_duration, data->S_steps[j]);
  }
  return max_duration;
}

bool parse_gltf(AnimationModel* model, const char* filename)
{

  Buffer buffer = {};
  sta_read_file(&buffer, filename);
  GLTF_Header header = *(GLTF_Header*)buffer.read(sizeof(GLTF_Header));

  GLTF_Chunk  chunk0 = {};
  GLTF_Chunk  chunk1 = {};
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

  // parse all bufferViews
  // translate to every accessor

  JsonValue*       scenes          = head->lookup_value("scenes");
  JsonValue*       animations      = head->lookup_value("animations");
  JsonValue*       meshes          = head->lookup_value("meshes");
  JsonValue*       skins           = head->lookup_value("skins");
  JsonValue*       buffers         = head->lookup_value("buffers");
  JsonValue*       bufferViews     = head->lookup_value("bufferViews");
  JsonValue*       accessors_value = head->lookup_value("accessors");

  GLTF_BufferView* buffer_views;
  u32              buffer_views_count;
  parse_buffer_views(&chunk1, &buffer_views, buffer_views_count, bufferViews);

  GLTF_Accessor* accessors;
  u32            accessor_count;
  parse_accessors(&accessors, accessor_count, accessors_value);

  // parse the correct order from skins
  // parse the inverse bind matrices

  JsonObject*      skin                             = skins->arr->values[0].obj;
  u32              inverse_bind_matrices_index      = skin->lookup_value("inverseBindMatrices")->number;
  GLTF_BufferView* inverse_bind_matrix_buffer_view  = &buffer_views[inverse_bind_matrices_index];
  f32*             inverseBindMatrices_buffer       = (f32*)inverse_bind_matrix_buffer_view->buffer;
  u32              inverseBindMatrices_buffer_index = 0;

  JsonArray*       joints_array                     = skin->lookup_value("joints")->arr;
  Skeleton*        skeleton                         = &model->skeleton;
  skeleton->joints                                  = (Joint*)sta_allocate_struct(Joint, joints_array->arraySize);
  u32* node_index_to_joint_index                    = (u32*)sta_allocate_struct(u32, joints_array->arraySize);
  skeleton->joint_count                             = joints_array->arraySize;
  for (u32 i = 0; i < joints_array->arraySize; i++)
  {
    Joint* joint = &skeleton->joints[i];
    for (u32 i = 0; i < 16; i++)
    {
      joint->m_invBindPose.m[i] = inverseBindMatrices_buffer[inverseBindMatrices_buffer_index * 16 + i];
    }
    joint->m_iParent = 0;
    inverseBindMatrices_buffer_index++;

    // grab index of node
    u32 node_index                        = joints_array->values[i].number;
    node_index_to_joint_index[node_index] = i;

    // grab the node
    JsonObject* node_object = json_nodes->arr->values[node_index].obj;
    JsonValue*  children    = node_object->lookup_value("children");
    if (children)
    {
      JsonArray* children_array = children->arr;
      for (u32 i = 0; i < children_array->arraySize; i++)
      {
        u32 child_idx                         = children_array->values[i].number;
        skeleton->joints[child_idx].m_iParent = i;
      }
    }

    // grab the name
    joint->m_name = node_object->lookup_value("name")->string;
    // add name, translation, rotation and scale
    JsonValue* translation = node_object->lookup_value("translation");
    Mat44      T           = {};
    T.identity();
    if (translation)
    {
      JsonArray* translation_array = translation->arr;
      T                            = Mat44::create_translation(Vector3(translation_array->values[0].number, translation_array->values[1].number, translation_array->values[2].number));
    }
    JsonValue* scale = node_object->lookup_value("scale");
    Mat44      S     = {};
    S.identity();
    if (scale)
    {
      JsonArray* scale_array = scale->arr;
      S                      = Mat44::create_scale(Vector3(scale_array->values[0].number, scale_array->values[1].number, scale_array->values[2].number));
    }
    JsonValue* rotation = node_object->lookup_value("rotation");
    Mat44      R        = {};
    R.identity();
    if (rotation)
    {
      JsonArray* rotation_array = rotation->arr;
      R = Mat44::create_rotation(Quaternion(rotation_array->values[0].number, rotation_array->values[1].number, rotation_array->values[2].number, rotation_array->values[3].number));
    }

    // do the matmuls to get m_mat
    // local transformation
    // global is from parent -> this node
    joint->m_mat = T.mul(R).mul(S);
  }
  for (u32 i = 0; i < skeleton->joint_count; i++)
  {
    Joint* joint = &skeleton->joints[i];
    joint->m_mat = skeleton->joints[joint->m_iParent].m_mat.mul(joint->m_mat);
  }

  // parse meshes
  JsonObject*   primitives             = meshes->arr->values[0].obj->lookup_value("primitives")->arr->values[0].obj;
  JsonObject*   attributes             = primitives->lookup_value("attributes")->obj;
  u32           indices_accessor_index = primitives->lookup_value("indices")->number;
  u32           position_index         = attributes->lookup_value("POSITION")->number;
  u32           normal_index           = attributes->lookup_value("NORMAL")->number;
  u32           texcoord_index         = attributes->lookup_value("TEXCOORD_0")->number;
  u32           joints_index           = attributes->lookup_value("JOINTS_0")->number;
  u32           weights_index          = attributes->lookup_value("WEIGHTS_0")->number;

  GLTF_Accessor indices_accessor       = accessors[indices_accessor_index];
  GLTF_Accessor position_accessor      = accessors[position_index];
  GLTF_Accessor normal_accessor        = accessors[normal_index];
  GLTF_Accessor texcoord_accessor      = accessors[texcoord_index];
  GLTF_Accessor joints_accessor        = accessors[joints_index];
  GLTF_Accessor weights_accessor       = accessors[weights_index];

  model->index_count                   = indices_accessor.count;
  model->indices                       = (u32*)sta_allocate_struct(u32, model->index_count);
  // it's short baby
  u16* index_buffer = (u16*)buffer_views[indices_accessor.buffer_view_index].buffer;
  assert(model->index_count * 2 == buffer_views[indices_accessor.buffer_view_index].buffer_length && "Indices doesn't equal buffer length?");
  for (u32 i = 0; i < model->index_count; i++)
  {
    model->indices[i] = index_buffer[i];
  }

  f32* position_buffer = (f32*)buffer_views[position_accessor.buffer_view_index].buffer;
  f32* normal_buffer   = (f32*)buffer_views[normal_accessor.buffer_view_index].buffer;
  f32* uv_buffer       = (f32*)buffer_views[texcoord_accessor.buffer_view_index].buffer;
  u8*  joints_buffer   = buffer_views[joints_accessor.buffer_view_index].buffer;
  f32* weights_buffer  = (f32*)buffer_views[weights_accessor.buffer_view_index].buffer;
  model->vertex_count  = position_accessor.count;
  model->vertices      = (SkinnedVertex*)sta_allocate_struct(SkinnedVertex, model->vertex_count);
  assert(model->vertex_count * 3 == buffer_views[position_accessor.buffer_view_index].buffer_length / 4);
  assert(model->vertex_count * 3 == buffer_views[normal_accessor.buffer_view_index].buffer_length / 4);
  assert(model->vertex_count * 2 == buffer_views[texcoord_accessor.buffer_view_index].buffer_length / 4);
  for (u32 i = 0; i < model->vertex_count; i++)
  {
    SkinnedVertex* vertex   = &model->vertices[i];
    u32            v4_index = i * 4;
    u32            v3_index = i * 3;
    u32            v2_index = i * 2;

    vertex->position        = Vector3(position_buffer[v3_index], position_buffer[v3_index + 1], position_buffer[v3_index + 2]);
    vertex->normal          = Vector3(normal_buffer[v3_index], normal_buffer[v3_index + 1], normal_buffer[v3_index + 2]);
    vertex->uv              = Vector2(uv_buffer[v2_index], -uv_buffer[v2_index + 1]);
    vertex->joint_index[0]  = joints_buffer[v4_index];
    vertex->joint_index[1]  = joints_buffer[v4_index + 1];
    vertex->joint_index[2]  = joints_buffer[v4_index + 2];
    vertex->joint_index[3]  = joints_buffer[v4_index + 3];

    vertex->joint_weight[0] = weights_buffer[v4_index];
    vertex->joint_weight[1] = weights_buffer[v4_index + 1];
    vertex->joint_weight[2] = weights_buffer[v4_index + 2];
    vertex->joint_weight[3] = weights_buffer[v4_index + 3];
  }

  JsonArray* animations_array   = animations->arr;
  model->clip_count             = animations_array->arraySize;
  JsonObject* animation_obj     = animations_array->values[0].obj;
  JsonArray*  channels          = animation_obj->lookup_value("channels")->arr;
  JsonArray*  samplers          = animation_obj->lookup_value("samplers")->arr;

  Animation*  animation         = &model->animations;
  animation->duration           = -1;
  animation->joint_count        = skeleton->joint_count;
  animation->poses              = (JointPose*)sta_allocate_struct(JointPose, skeleton->joint_count);

  AnimationData* animation_data = (AnimationData*)sta_allocate_struct(AnimationData, animation->joint_count);

  for (u32 i = 0; i < channels->arraySize; i++)
  {
    JsonObject*            channel       = channels->values[i].obj;
    JsonObject*            target        = channel->lookup_value("target")->obj;
    u32                    node_index    = target->lookup_value("node")->number;
    GLTF_ChannelTargetPath path          = get_channel_target_path(target->lookup_value("path")->string);
    u32                    sampler_index = channel->lookup_value("sampler")->number;

    JsonObject*            sampler       = samplers->values[sampler_index].obj;
    u32                    input_index   = sampler->lookup_value("input")->number;
    u32                    output_index  = sampler->lookup_value("output")->number;
    // the times of the key frames of the animation
    GLTF_Accessor* input       = &accessors[input_index];
    f32*           steps_array = (f32*)buffer_views[input->buffer_view_index].buffer;
    // values for the animated property at the respective key frames
    GLTF_Accessor* output      = &accessors[output_index];
    f32*           data_array  = (f32*)buffer_views[output->buffer_view_index].buffer;

    u32            joint_index = node_index_to_joint_index[node_index];

    AnimationData* data        = &animation_data[joint_index];
    if (path == CHANNEL_TARGET_PATH_TRANSLATION)
    {
      data->T_count = input->count;
      data->T       = (Mat44*)sta_allocate_struct(Mat44, input->count);
      data->T_steps = (f32*)sta_allocate_struct(f32, input->count);
      for (u32 j = 0; j < input->count; j++)
      {
        u32 data_index   = j * 3;
        data->T_steps[j] = steps_array[j];
        data->T[j]       = Mat44::create_translation(Vector3(data_array[data_index], data_array[data_index + 1], data_array[data_index + 2]));
      }
    }
    else if (path == CHANNEL_TARGET_PATH_ROTATION)
    {
      data->R_count = input->count;
      data->R       = (Mat44*)sta_allocate_struct(Mat44, input->count);
      data->R_steps = (f32*)sta_allocate_struct(f32, input->count);
      for (u32 j = 0; j < input->count; j++)
      {
        u32 data_index   = j * 3;
        data->R_steps[j] = steps_array[j];
        data->R[j]       = Mat44::create_rotation(Quaternion(data_array[data_index], data_array[data_index + 1], data_array[data_index + 2], data_array[data_index + 3]));
      }
    }
    else if (path == CHANNEL_TARGET_PATH_SCALE)
    {
      data->S_count = input->count;
      data->S       = (Mat44*)sta_allocate_struct(Mat44, input->count);
      data->S_steps = (f32*)sta_allocate_struct(f32, input->count);
      for (u32 j = 0; j < input->count; j++)
      {
        u32 data_index   = j * 3;
        data->S_steps[j] = steps_array[j];
        data->S[j]       = Mat44::create_scale(Vector3(data_array[data_index], data_array[data_index + 1], data_array[data_index + 2]));
      }
    }
    else
    {
      assert(!"Unknown channel target!");
    }
  }

  animation->duration = 0.0f;
  for (u32 i = 0; i < skeleton->joint_count; i++)
  {
    LL steps[animation_data->T_count + animation_data->S_count + animation_data->R_count];
    memset(steps, 0, sizeof(LL) * (animation_data->T_count + animation_data->S_count + animation_data->R_count));
    JointPose* pose       = &animation->poses[i];
    pose->step_count      = 0;
    AnimationData* data   = &animation_data[i];
    animation->duration   = MAX(calculate_steps(data, steps, pose->step_count), animation->duration);

    pose->steps           = (f32*)sta_allocate_struct(f32, pose->step_count);
    pose->local_transform = (Mat44*)sta_allocate_struct(f32, pose->step_count);
    LL* head              = &steps[0];
    u32 count             = 0;
    while (head)
    {
      pose->steps[count] = head->time;
      Mat44* local       = &pose->local_transform[count];
      local->identity();

      // check if T
      for (u32 i = 0; i < data->T_count; i++)
      {
        if (compare_float(data->T_steps[i], head->time))
        {
          pose->local_transform[count] = local->mul(data->T[i]);
          break;
        }
      }

      // check if R
      for (u32 i = 0; i < data->R_count; i++)
      {
        if (compare_float(data->R_steps[i], head->time))
        {
          pose->local_transform[count] = local->mul(data->R[i]);
          break;
        }
      }

      // check if S
      for (u32 i = 0; i < data->S_count; i++)
      {
        if (compare_float(data->S_steps[i], head->time))
        {
          pose->local_transform[count] = local->mul(data->S[i]);
          break;
        }
      }

      head = head->next;
      count++;
    }
    assert(count == pose->step_count && "Really");
  }

  return true;
}

#undef GLTF_CHUNK_TYPE_JSON
