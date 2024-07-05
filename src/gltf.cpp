#include "gltf.h"

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

  for (u32 i = 0; i < joints_array->arraySize; i++)
  {
    Joint* joint = &skeleton->joints[i];
    memcpy(&joint->m_invBindPose.m[0], &ibm_buffer[ibm_buffer_index * 16], 16 * sizeof(float));
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

  Mat44 m              = {};
  m.identity();
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
    vertex->position        = transform_vertex_to_y_up(m, vertex->position.x, vertex->position.y, vertex->position.z);
    vertex->normal          = Vector3(normal_buffer[v3_index], normal_buffer[v3_index + 1], normal_buffer[v3_index + 2]);
    vertex->normal          = transform_vertex_to_y_up(m_inv, vertex->normal.x, vertex->normal.y, vertex->normal.z);
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
  JsonArray* animations_array        = animations->arr;
  model->clip_count                  = animations_array->arraySize;
  JsonObject* animation_obj          = animations_array->values[0].obj;
  JsonArray*  channels               = animation_obj->lookup_value("channels")->arr;
  JsonArray*  samplers               = animation_obj->lookup_value("samplers")->arr;

  Animation*  animation              = &model->animations;
  animation->duration                = -1;
  animation->joint_count             = skeleton->joint_count;
  animation->poses                   = (JointPose*)sta_allocate_struct(JointPose, skeleton->joint_count);

  GLTF_AnimationData* animation_data = (GLTF_AnimationData*)sta_allocate_struct(GLTF_AnimationData, animation->joint_count);

  Mat44               m              = {};
  m.identity();
  m = m.rotate_x(90);
  for (u32 i = 0; i < channels->arraySize; i++)
  {
    JsonObject* channel       = channels->values[i].obj;
    JsonObject* target        = channel->lookup_value("target")->obj;
    u32         node_index    = target->lookup_value("node")->number;
    u32         sampler_index = channel->lookup_value("sampler")->number;

    JsonObject* sampler       = samplers->values[sampler_index].obj;
    u32         input_index   = sampler->lookup_value("input")->number;
    u32         output_index  = sampler->lookup_value("output")->number;
    // the times of the key frames of the animation
    GLTF_Accessor* input       = &accessors[input_index];
    f32*           steps_array = (f32*)buffer_views[input->buffer_view_index].buffer;
    // values for the animated property at the respective key frames
    GLTF_Accessor*         output      = &accessors[output_index];
    f32*                   data_array  = (f32*)buffer_views[output->buffer_view_index].buffer;

    u32                    joint_index = node_index_to_joint_index[node_index];
    GLTF_ChannelTargetPath path        = get_channel_target_path(target->lookup_value("path")->string);

    // Store this just as T,R,S instead :)
    GLTF_AnimationData* data = &animation_data[joint_index];
    if (path == CHANNEL_TARGET_PATH_TRANSLATION)
    {
      data->T_count = input->count;
      data->T       = (Vector3*)sta_allocate_struct(Vector3, input->count);
      data->T_steps = (f32*)sta_allocate_struct(f32, input->count);
      for (u32 j = 0; j < input->count; j++)
      {
        u32 data_index   = j * 3;
        data->T_steps[j] = steps_array[j];
        data->T[j]       = Vector3(data_array[data_index], data_array[data_index + 1], data_array[data_index + 2]);
      }
    }
    else if (path == CHANNEL_TARGET_PATH_ROTATION)
    {
      data->R_count = input->count;
      data->R       = (Quaternion*)sta_allocate_struct(Quaternion, input->count);
      data->R_steps = (f32*)sta_allocate_struct(f32, input->count);
      for (u32 j = 0; j < input->count; j++)
      {
        u32 data_index   = j * 4;
        data->R_steps[j] = steps_array[j];
        data->R[j]       = Quaternion(data_array[data_index], data_array[data_index + 1], data_array[data_index + 2], data_array[data_index + 3]);
      }
    }
    else if (path == CHANNEL_TARGET_PATH_SCALE)
    {
      data->S_count = input->count;
      data->S       = (Vector3*)sta_allocate_struct(Vector3, input->count);
      data->S_steps = (f32*)sta_allocate_struct(f32, input->count);
      for (u32 j = 0; j < input->count; j++)
      {
        u32 data_index   = j * 3;
        data->S_steps[j] = steps_array[j];
        data->S[j]       = Vector3(data_array[data_index], data_array[data_index + 1], data_array[data_index + 2]);
      }
    }
    else
    {
      assert(!"Unknown channel target!");
    }
  }

  animation->duration = 0.0f;
  i32 empty[skeleton->joint_count];
  u32 empty_count = 0;
  for (u32 i = 0; i < skeleton->joint_count; i++)
  {
    JointPose* pose = &animation->poses[i];
    if (animation_data[i].T_count + animation_data[i].S_count + animation_data[i].R_count == 0)
    {
      pose->step_count      = 2;
      pose->steps           = (f32*)sta_allocate_struct(f32, pose->step_count);
      pose->steps[0]        = 0;
      pose->local_transform = (Mat44*)sta_allocate_struct(Mat44, pose->step_count);
      pose->local_transform[0].identity();
      pose->local_transform[1].identity();
      empty[empty_count++] = i;
      continue;
    }

    LL* steps = (LL*)sta_allocate_struct(LL, animation_data[i].T_count + animation_data[i].S_count + animation_data[i].R_count + 1);
    memset(steps, 0, sizeof(LL) * (animation_data->T_count + animation_data->S_count + animation_data->R_count));
    pose->step_count         = 0;
    GLTF_AnimationData* data = &animation_data[i];

    animation->duration      = MAX(calculate_steps(data, steps, pose->step_count), animation->duration);

    pose->steps              = (f32*)sta_allocate_struct(f32, pose->step_count);
    pose->local_transform    = (Mat44*)sta_allocate_struct(Mat44, pose->step_count);
    LL* head                 = &steps[0];
    if (head->prev)
    {
      head = head->prev;
    }
    u32 count = 0;
    while (head)
    {
      pose->steps[count] = head->time;
      f32 current_time   = head->time;
      head               = head->next;
      pose->local_transform[count].identity();
      // insert and interpolate T
      u32 j = 0;
      for (; j < data->T_count; j++)
      {
        if (data->T_steps[j] > current_time || compare_float(data->T_steps[j], current_time))
        {
          break;
        }
      }

      if (j == data->T_count)
      {
        j--;
      }
      else if (j == 0)
      {
        j++;
      }
      f32     t                    = (current_time - data->T_steps[j - 1]) / (data->T_steps[j] - data->T_steps[j - 1]);
      Vector3 translated           = interpolate_translation(data->T[j - 1], data->T[j], t);
      pose->local_transform[count] = pose->local_transform[count].mul(Mat44::create_translation(translated));

      // check if R
      for (j = 0; j < data->R_count; j++)
      {
        if (data->R_steps[j] > current_time || compare_float(data->R_steps[j], current_time))
        {
          break;
        }
      }
      if (j == data->R_count)
      {
        j--;
      }
      else if (j == 0)
      {
        j++;
      }
      t                            = (current_time - data->R_steps[j - 1]) / (data->R_steps[j] - data->R_steps[j - 1]);
      Quaternion q                 = Quaternion::interpolate_linear(data->R[j - 1], data->R[j], t);
      pose->local_transform[count] = pose->local_transform[count].mul(Mat44::create_rotation(q));

      // check if S
      for (j = 0; j < data->S_count; j++)
      {
        if (data->S_steps[j] > current_time || compare_float(data->S_steps[j], current_time))
        {
          break;
        }
      }
      if (j == data->S_count)
      {
        j--;
      }
      else if (j == 0)
      {
        j++;
      }

      t                            = (current_time - data->S_steps[j - 1]) / (data->S_steps[j] - data->S_steps[j - 1]);
      Vector3 scaled               = interpolate_translation(data->S[j - 1], data->S[j], t);
      pose->local_transform[count] = pose->local_transform[count].mul(Mat44::create_scale(scaled));

      count++;
    }
  }
  for (u32 i = 0; i < empty_count; i++)
  {
    animation->poses[empty[i]].steps[1] = animation->duration;
  }
}

bool gltf_parse(AnimationModel* model, const char* filename)
{

  Buffer buffer = {};
  if(!sta_read_file(&buffer, filename)){
    logger.error("Couldn't find file '%s'", filename);
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
  // sta_json_serialize_to_file(&json_data, "full.json");

  JsonObject* head       = &json_data.obj;
  JsonValue*  json_nodes = head->lookup_value("nodes");
  assert(json_nodes->type == JSON_ARRAY && "Nodes should be an array?");

  // JsonValue*       scenes          = head->lookup_value("scenes");
  JsonValue*       animations      = head->lookup_value("animations");
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
