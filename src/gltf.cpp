#include "gltf.h"
#include "platform.h"
#include <assert.h>

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

static f32 calculate_steps(GLTF_AnimationData* data, LL* steps, u32& step_count)
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
  if (steps[0].time > 0)
  {
    LL* first     = &steps[step_count++];
    first->next   = &steps[0];
    first->prev   = 0;
    first->time   = 0;
    steps[0].prev = first;
  }

  return max_duration;
}

static void parse_skeleton(Skeleton* skeleton, JsonValue* skins, u32** node_index_to_joint_index, GLTF_BufferView* buffer_views, JsonValue* json_nodes)
{

  JsonObject* skin             = skins->arr->values[0].obj;

  f32*        ibm_buffer       = (f32*)buffer_views[(u32)skin->lookup_value("inverseBindMatrices")->number].buffer;
  u32         ibm_buffer_index = 0;

  JsonArray*  joints_array     = skin->lookup_value("joints")->arr;
  skeleton->joint_count        = joints_array->arraySize;
  skeleton->joints             = (Joint*)sta_allocate_struct(Joint, skeleton->joint_count);
  *node_index_to_joint_index   = (u32*)sta_allocate_struct(u32, skeleton->joint_count);

  Mat44 m                      = Mat44::identity();
  m = m.rotate_x(90);
  for (u32 i = 0; i < joints_array->arraySize; i++)
  {
    Joint* joint = &skeleton->joints[i];
    memcpy(&joint->m_invBindPose.m[0], &ibm_buffer[ibm_buffer_index * 16], 16 * sizeof(float));
    joint->m_invBindPose.debug();
    printf("-\n");

    ibm_buffer_index++;

    u32 node_index                           = joints_array->values[i].number;
    (*node_index_to_joint_index)[node_index] = i;

    JsonObject* node_object                  = json_nodes->arr->values[node_index].obj;

    joint->m_name                            = node_object->lookup_value("name")->string;
  }

  for (u32 i = 0; i < joints_array->arraySize; i++)
  {
    u32         node_index  = joints_array->values[i].number;
    JsonObject* node_object = json_nodes->arr->values[node_index].obj;
    JsonValue*  children    = node_object->lookup_value("children");
    if (children)
    {
      JsonArray* children_array = children->arr;
      for (u32 j = 0; j < children_array->arraySize; j++)
      {
        u32 child_idx                                                       = children_array->values[j].number;
        skeleton->joints[(*node_index_to_joint_index)[child_idx]].m_iParent = i;
      }
    }
  }
}
static Vector3 transform_vertex_to_y_up(Mat44 m, f32 x, f32 y, f32 z)
{
  return m.mul(Vector4(x, y, z, 1.0)).project();
}

static void parse_mesh(AnimationModel* model, GLTF_Accessor* accessors, JsonValue* meshes, GLTF_BufferView* buffer_views)
{
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

  Mat44 m              =Mat44::identity();
  m           = m.rotate_x(90);

  Mat44 m_inv = m.inverse();
  m_inv.transpose();

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
}

static void parse_animations(AnimationModel* model, Skeleton* skeleton, JsonValue* animations, GLTF_Accessor* accessors, GLTF_BufferView* buffer_views, u32* node_index_to_joint_index)
{
  JsonArray* animations_array = animations->arr;
  model->animation_count      = animations_array->arraySize;
  model->animations           = sta_allocate_struct(Animation, model->animation_count);
  for (u32 i = 0; i < model->animation_count; i++)
  {
    JsonObject* animation_obj = animations_array->values[i].obj;
    model->animations[i].name = animation_obj->lookup_value("name")->string;

    JsonArray* channels       = animation_obj->lookup_value("channels")->arr;
    JsonArray* samplers       = animation_obj->lookup_value("samplers")->arr;

    struct ScaleNode
    {
      Vector3 scale;
      f32     time;
    };
    struct TranslationNode
    {
      Vector3 translation;
      f32     time;
    };
    struct RotationNode
    {
      Quaternion rotation;
      f32        time;
    };

    struct AnimationDataNode
    {
      ScaleNode*       scale;
      TranslationNode* translation;
      RotationNode*    rotation;
      u32              scale_count;
      u32              translation_count;
      u32              rotation_count;
    };

    AnimationDataNode* animation_data = {};
    animation_data                    = sta_allocate_struct(AnimationDataNode, skeleton->joint_count);
    memset(animation_data, 0, sizeof(AnimationDataNode) * skeleton->joint_count);

    // for each joint
    // store separate s,r,t data
    // then concatenate them

    for (u32 channel_index = 0; channel_index < channels->arraySize; channel_index++)
    {
      JsonObject*            channel              = channels->values[channel_index].obj;
      JsonObject*            sampler              = samplers->values[(u32)channel->lookup_value("sampler")->number].obj;
      JsonObject*            target               = channel->lookup_value("target")->obj;
      u32                    joint_index          = node_index_to_joint_index[(u32)target->lookup_value("node")->number];

      GLTF_Accessor          input_accessor       = accessors[(u32)sampler->lookup_value("input")->number];
      f32*                   steps_buffer         = (f32*)buffer_views[input_accessor.buffer_view_index].buffer;

      GLTF_Accessor          output_accessor      = accessors[(u32)sampler->lookup_value("output")->number];
      f32*                   output_buffer        = (f32*)buffer_views[output_accessor.buffer_view_index].buffer;

      GLTF_ChannelTargetPath path                 = get_channel_target_path(target->lookup_value("path")->string);

      AnimationDataNode*     joint_animation_data = &animation_data[joint_index];

      switch (path)
      {
      case CHANNEL_TARGET_PATH_SCALE:
      {
        joint_animation_data->scale_count = input_accessor.count;
        if (joint_animation_data->scale_count != 0)
        {
          joint_animation_data->scale = sta_allocate_struct(ScaleNode, input_accessor.count);
          for (u32 j = 0; j < input_accessor.count; j++)
          {
            joint_animation_data->scale[j].time    = steps_buffer[j];
            u32 scale_index                        = j * 3;
            joint_animation_data->scale[j].scale.x = output_buffer[scale_index + 0];
            joint_animation_data->scale[j].scale.y = output_buffer[scale_index + 1];
            joint_animation_data->scale[j].scale.z = output_buffer[scale_index + 2];
          }
        }
        break;
      }
      case CHANNEL_TARGET_PATH_ROTATION:
      {
        joint_animation_data->rotation_count = output_accessor.count;
        if (joint_animation_data->rotation_count != 0)
        {
          joint_animation_data->rotation = sta_allocate_struct(RotationNode, input_accessor.count);
          for (u32 j = 0; j < input_accessor.count; j++)
          {
            joint_animation_data->rotation[j].time       = steps_buffer[j];
            u32 scale_index                              = j * 4;
            joint_animation_data->rotation[j].rotation.x = output_buffer[scale_index + 0];
            joint_animation_data->rotation[j].rotation.y = output_buffer[scale_index + 1];
            joint_animation_data->rotation[j].rotation.z = output_buffer[scale_index + 2];
            joint_animation_data->rotation[j].rotation.w = output_buffer[scale_index + 3];
          }
        }
        break;
      }
      case CHANNEL_TARGET_PATH_TRANSLATION:
      {
        joint_animation_data->translation_count = output_accessor.count;
        if (joint_animation_data->translation_count != 0)
        {
          joint_animation_data->translation = sta_allocate_struct(TranslationNode, input_accessor.count);
          for (u32 j = 0; j < input_accessor.count; j++)
          {
            joint_animation_data->translation[j].time          = steps_buffer[j];
            u32 scale_index                                    = j * 3;
            joint_animation_data->translation[j].translation.x = output_buffer[scale_index + 0];
            joint_animation_data->translation[j].translation.y = output_buffer[scale_index + 1];
            joint_animation_data->translation[j].translation.z = output_buffer[scale_index + 2];
          }
        }
        break;
      }
      }
    }

    Animation* animation   = &model->animations[i];
    animation->joint_count = skeleton->joint_count;
    animation->poses       = sta_allocate_struct(JointPose, skeleton->joint_count);
    f32   duration         = -FLT_MAX;

    Mat44 m                = Mat44::identity();
    m = m.rotate_x(90);
    for (u32 j = 0; j < skeleton->joint_count; j++)
    {
      JointPose*        pose = &animation->poses[j];
      AnimationDataNode data = animation_data[j];
      if (data.translation_count == 0)
      {
        pose->step_count       = 0;
        pose->steps            = 0;
        pose->local_transforms = 0;
        continue;
      }
      assert(data.scale_count == data.rotation_count && data.scale_count == data.translation_count && "Implement me please, don't optimize this export xD");
      duration               = MAX(data.translation[data.translation_count - 1].time, duration);

      pose->step_count       = data.translation_count;
      // pose->local_transforms = sta_allocate_struct(SRT, pose->step_count);
      pose->steps            = sta_allocate_struct(f32, pose->step_count);
      for (u32 k = 0; k < pose->step_count; k++)
      {
        pose->steps[k]                        = data.translation[k].time;
        // pose->local_transforms[k].translation = data.translation[k].translation;
        // Vector3 t                             = pose->local_transforms[k].translation;
        // pose->local_transforms[k].translation = transform_vertex_to_y_up(m, t.x, t.y, t.z);

        // pose->local_transforms[k].rotation    = data.rotation[k].rotation;
        // pose->local_transforms[k].scale       = data.scale[k].scale;
      }
    }
    animation->duration = duration;
  }
}

bool gltf_parse(AnimationModel* model, const char* filename)
{

  Buffer buffer = {};
  if (!sta_read_file(&buffer, filename))
  {
    printf("Couldn't find file '%s'", filename);
    return false;
  }
  (void)buffer.read(sizeof(GLTF_Header));

  GLTF_Chunk chunk0 = {};
  GLTF_Chunk chunk1 = {};
  read_chunk(&buffer, &chunk0);
  read_chunk(&buffer, &chunk1);

  Json   json_data = {};
  Buffer buffer_chunk0((char*)chunk0.chunk_data, chunk0.chunk_length);
  bool   result = sta_json_deserialize_from_string(&buffer_chunk0, &json_data);
  if (!result)
  {
    printf("Failed to deserialize json!\n");
    return false;
  }
  sta_json_serialize_to_file(&json_data, "full.json");

  JsonObject* head       = &json_data.obj;
  JsonValue*  json_nodes = head->lookup_value("nodes");
  assert(json_nodes->type == JSON_ARRAY && "Nodes should be an array?");

  // JsonValue*       scenes          = head->lookup_value("scenes");
  JsonValue* animations     = head->lookup_value("animations");
  Json       animation_json = {};
  animation_json.headType   = JSON_VALUE;
  animation_json.value      = *animations;
  sta_json_serialize_to_file(&animation_json, "animation.json");
  JsonValue*       meshes          = head->lookup_value("meshes");
  JsonValue*       skins           = head->lookup_value("skins");
  JsonValue*       bufferViews     = head->lookup_value("bufferViews");
  JsonValue*       accessors_value = head->lookup_value("accessors");

  GLTF_BufferView* buffer_views;
  u32              buffer_views_count;
  parse_buffer_views(&chunk1, &buffer_views, buffer_views_count, bufferViews);

  GLTF_Accessor* accessors;
  u32            accessor_count;
  parse_accessors(&accessors, accessor_count, accessors_value);

  Skeleton* skeleton = &model->skeleton;
  u32*      node_index_to_joint_index;
  parse_skeleton(skeleton, skins, &node_index_to_joint_index, buffer_views, json_nodes);

  parse_mesh(model, accessors, meshes, buffer_views);

  parse_animations(model, skeleton, animations, accessors, buffer_views, node_index_to_joint_index);

  return true;
}

#undef GLTF_CHUNK_TYPE_JSON
