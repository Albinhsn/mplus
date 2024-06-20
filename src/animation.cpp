#include "animation.h"
#include "common.h"
#include "files.h"
#include "platform.h"
#include "shader.h"
#include "vector.h"
#include <cassert>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void SkinnedVertex::debug()
{
  this->position.debug();
  this->normal.debug();
  this->uv.debug();
  printf("%d %d %d %d\n", this->joint_index[0], this->joint_index[1], this->joint_index[2], this->joint_index[3]);
  printf("%f %f %f %f\n", this->joint_weight[0], this->joint_weight[1], this->joint_weight[2], this->joint_weight[3]);
}
void AnimationModel::debug()
{
  printf("Vertices: %ld\n", this->vertex_count);
  for (u32 i = 0; i < this->vertex_count; i++)
  {
    this->vertices[i].debug();
    printf("-\n");
  }
}

void calculate_new_pose(AnimationModel anim, Mat44* poses, u32 count, Animation animation, u32 ticks)
{
  const f32 EPSILON   = 0.0001f;
  u64       loop_time = (u64)(1000 * animation.duration);
  f32       time      = (ticks % loop_time) / (f32)loop_time;
  if (time == 0)
  {
    time += EPSILON;
  }

  for (u32 i = 0; i < animation.joint_count; i++)
  {
    if (time >= animation.duration - EPSILON)
    {
      time = animation.duration;
    }
    u32        pose_idx = 0;
    JointPose* pose     = &animation.poses[i];
    for (; pose_idx < pose->step_count && pose->steps[pose_idx] < time; pose_idx++)
      ;

    float time_between_poses = (time - pose->steps[pose_idx - 1]) / (pose->steps[pose_idx] - pose->steps[pose_idx - 1]);

    Mat44 first_transform    = pose->local_transform[pose_idx - 1];
    Mat44 second_transform   = pose->local_transform[pose_idx];
    poses[i]                 = interpolate_transforms(first_transform, second_transform, time_between_poses);
  }
}

void update_animation(AnimationModel animation, Shader shader, u32 ticks)
{
  Skeleton* skeleton = &animation.skeleton;
  Mat44     transforms[skeleton->joint_count];
  Mat44     current_poses[skeleton->joint_count];
  calculate_new_pose(animation, current_poses, skeleton->joint_count, animation.animations, ticks);

  Mat44 parent_transforms[skeleton->joint_count];
  for (int i = 0; i < skeleton->joint_count; i++)
  {
    parent_transforms[i].identity();
  }
  for (u32 i = 0; i < skeleton->joint_count; i++)
  {
    Joint* joint             = &skeleton->joints[i];

    Mat44  current_local     = current_poses[i];
    Mat44  parent_transform  = parent_transforms[joint->m_iParent];

    Mat44  current_transform = parent_transform.mul(current_local);
    parent_transforms[i]     = current_transform;
    transforms[i]            = current_transform.mul(joint->m_invBindPose);
  }
  shader.set_mat4("jointTransforms", &transforms[0].m[0], skeleton->joint_count);
}

static Buffer sta_xml_create_buffer_from_content(XML_Node* node)
{
  Buffer buffer = {};
  buffer.buffer = (char*)node->content.buffer;
  buffer.index  = 0;
  buffer.len    = node->content.length;
  return buffer;
}

static int sta_xml_get_count_from_tag(XML_Node* node)
{
  XML_Tag* node_tag = node->find_tag("count");
  Buffer   buffer(node_tag->value, node_tag->value_length);
  return buffer.parse_int();
}

static u8 get_joint_index_from_id(char** names, u32 count, char* name, u64 length)
{
  for (unsigned int i = 0; i < count; i++)
  {
    if (strlen(names[i]) == length && strncmp(name, names[i], length) == 0)
    {
      return i;
    }
  }
  assert(0 && "how could this happen to me");
}
static u8 get_joint_index_from_id(Skeleton* skeleton, char* name, u64 length)
{
  for (unsigned int i = 0; i < skeleton->joint_count; i++)
  {
    if (skeleton->joints[i].m_name_length == length && strncmp(name, skeleton->joints[i].m_name, length) == 0)
    {
      return i;
    }
  }
  assert(0 && "Couldn't find joint by this name?");
}

struct JointData
{
  int        index;
  char*      name;
  u32        name_length;
  Mat44      local_bind_transform;
  Mat44      inverse_bind_transform;
  JointData* children;
  u32        children_cap;
  u32        children_count;
};

struct SkeletonData
{
  int        joint_count;
  JointData* head_joint;
};

struct JointWeight
{
  u32 joint_id;
  f32 weight;
};

struct VertexSkinData
{
  JointWeight* data;
  u32          count;
};

struct SkinningData
{
  char**          joint_order;
  u32             joint_order_count;
  VertexSkinData* vertex_skin_data;
  u32             vertex_skin_data_count;
};

struct MeshData
{
  SkinnedVertex* vertices;
  u32*           indices;
  u32            vertex_count;
};

struct JointTransformData
{
  char* name;
  u32   length;
  Mat44 local_transform;
};
struct KeyFrameData
{
  float               time;
  JointTransformData* transforms;
};

struct AnimationData
{
  KeyFrameData* key_frames;
  float*        timesteps;
  float         duration;
  u32           count;
};

struct ColladaModelData
{
  SkeletonData  skeleton_data;
  MeshData      mesh_data;
  SkinningData  skinning_data;
  AnimationData animation_data;
};

static int cmp_vertex_skin_data(const void* _a, const void* _b)
{
  JointWeight* a = (JointWeight*)_a;
  JointWeight* b = (JointWeight*)_b;
  return a->weight - b->weight;
}

static void get_skin_data(VertexSkinData* vertex_skin_data, u32 vertex_skin_count, i32* vcount, i32 vcount_count, f32* weights, XML_Node* vertex_weights)
{
  XML_Node* v = vertex_weights->find_key("v");
  Buffer    buff(v->content.buffer, v->content.length);
  Buffer*   buffer = &buff;

  for (u32 i = 0; i < vcount_count; i++)
  {
    VertexSkinData* skin_data = &vertex_skin_data[i];
    i32             vc        = vcount[i];
    skin_data->count          = MIN(vc, 4);
    skin_data->data           = (JointWeight*)sta_allocate_struct(JointWeight, skin_data->count);
    for (u32 j = 0; j < vc; j++)
    {
      buffer->skip_whitespace();
      i32 joint_id = parse_int_from_string(buffer);
      buffer->skip_whitespace();
      i32 weight_id = parse_int_from_string(buffer);
      if (j < 4)
      {
        skin_data->data[j].weight   = weights[weight_id];
        skin_data->data[j].joint_id = joint_id;
      }
    }

    qsort(skin_data, sizeof(JointWeight), skin_data->count, cmp_vertex_skin_data);

    if (vc > 4)
    {
      float sum = 0.0f;
      for (u32 j = 0; j < 4; j++)
      {
        sum += skin_data->data[j].weight;
      }
      float after_sum = 0.0f;
      for (u32 j = 0; j < 4; j++)
      {
        skin_data->data[j].weight /= sum;
        after_sum += skin_data->data[j].weight;
      }
    }
  }
}

static void extract_skin_data(SkinningData* skinning_data, XML_Node* controllers)
{
  XML_Node* skin           = controllers->find_key("controller")->find_key("skin");
  XML_Node* vertex_weights = skin->find_key("vertex_weights");
  XML_Tag*  joint_id_tag   = vertex_weights->find_key_with_tag("input", "semantic", "JOINT")->find_tag("source");

  String    joint_id(&joint_id_tag->value[1], joint_id_tag->value_length - 1);

  XML_Node* joint_node             = skin->find_key_with_tag("source", "id", joint_id)->find_key("Name_array");

  u32       count                  = sta_xml_get_count_from_tag(joint_node);
  skinning_data->joint_order       = (char**)sta_allocate_struct(char*, count);
  skinning_data->joint_order_count = count;
  Buffer string_array_buffer(joint_node->content.buffer, joint_node->content.length);
  string_array_buffer.parse_string_array((char**)skinning_data->joint_order, count);

  XML_Tag*  weights_id_tag = vertex_weights->find_key_with_tag("input", "semantic", "WEIGHT")->find_tag("source");

  String    weights_id(&weights_id_tag->value[1], weights_id_tag->value_length - 1);

  XML_Node* weight_node  = skin->find_key_with_tag("source", "id", weights_id)->find_key("float_array");
  u32       weight_count = sta_xml_get_count_from_tag(weight_node);
  float*    weights      = (f32*)sta_allocate_struct(f32, weight_count);
  Buffer    buffer(weight_node->content.buffer, weight_node->content.length);

  buffer.parse_float_array(weights, weight_count);

  u32 vertex_weights_count              = sta_xml_get_count_from_tag(vertex_weights);
  skinning_data->vertex_skin_data_count = vertex_weights_count;
  skinning_data->vertex_skin_data       = (VertexSkinData*)sta_allocate_struct(VertexSkinData, vertex_weights_count);
  i32*      vcount                      = (i32*)sta_allocate_struct(i32, vertex_weights_count);
  XML_Node* vcount_node                 = vertex_weights->find_key("vcount");

  buffer.buffer                         = (char*)vcount_node->content.buffer;
  buffer.len                            = vcount_node->content.length;
  buffer.index                          = 0;

  buffer.parse_int_array(vcount, vertex_weights_count);

  get_skin_data(skinning_data->vertex_skin_data, vertex_weights_count, vcount, vertex_weights_count, weights, vertex_weights);
}

static Mat44 convert_z_to_y_up()
{
  Mat44 res;
  res.identity();
  res = res.rotate_x(-90);
  res.transpose();
  return res;
}

static Mat44 CORRECTION = convert_z_to_y_up();

static void  extract_main_joint_data(JointData* joint, XML_Node* child, bool is_root, char** bone_order, u32 bone_count)
{
  XML_Tag* name_tag     = child->find_tag("id");
  u32      idx          = get_joint_index_from_id(bone_order, bone_count, name_tag->value, name_tag->value_length);

  joint->name           = name_tag->value;
  joint->name_length    = name_tag->value_length;
  joint->index          = idx;

  XML_Node* matrix_node = child->find_key("matrix");
  Buffer    buffer      = sta_xml_create_buffer_from_content(matrix_node);

  buffer.parse_float_array(&joint->local_bind_transform.m[0], 16);
  joint->local_bind_transform.transpose();
}

static void load_joint_data(JointData* joint, XML_Node* node, bool is_root, SkinningData* skinning_data)
{
  const int length_of_node = 4;
  extract_main_joint_data(joint, node, is_root, skinning_data->joint_order, skinning_data->joint_order_count);

  XML_Node* child_node = node->child;
  while (child_node)
  {
    if (child_node->header.name_length == length_of_node && strncmp(child_node->header.name, "node", 4) == 0)
    {
      RESIZE_ARRAY(joint->children, JointData, joint->children_count, joint->children_cap);
      JointData* child_joint      = &joint->children[joint->children_count++];
      child_joint->children_cap   = 1;
      child_joint->children       = (JointData*)sta_allocate_struct(JointData, 1);
      child_joint->children_count = 0;
      load_joint_data(child_joint, child_node, false, skinning_data);
    }

    child_node = child_node->next;
  }
}

static void extract_bone_data(SkeletonData* skeleton_data, XML_Node* visual_scenes, SkinningData* skinning_data)
{
  XML_Node* armature_data                   = visual_scenes->find_key("visual_scene");
  skeleton_data->joint_count                = skinning_data->joint_order_count;
  XML_Node* child                           = armature_data->find_key_with_tag("node", "id", "mixamorig_Hips");
  skeleton_data->head_joint                 = (JointData*)sta_allocate_struct(JointData, 1);
  skeleton_data->head_joint->children_cap   = 1;
  skeleton_data->head_joint->children       = (JointData*)sta_allocate_struct(JointData, 1);
  skeleton_data->head_joint->children_count = 0;
  load_joint_data(skeleton_data->head_joint, child, true, skinning_data);
}

static void read_positions(XML_Node* mesh_node, Vector3** positions, u32& count, VertexSkinData* skin_data)
{

  XML_Tag*  source_tag = mesh_node->find_key("vertices")->find_key("input")->find_tag("source");
  String    positions_id(&source_tag->value[1], source_tag->value_length - 1);

  XML_Node* positions_data = mesh_node->find_key_with_tag("source", "id", positions_id)->find_key("float_array");

  count                    = sta_xml_get_count_from_tag(positions_data);
  Vector3* pos             = (Vector3*)sta_allocate_struct(Vector3, count);
  Buffer   buffer          = sta_xml_create_buffer_from_content(positions_data);

  buffer.parse_vector3_array(pos, count);

  for (int i = 0; i < count; i++)
  {
    Vector3* v  = &pos[i];

    Vector4  v4 = Vector4(v->x, v->y, v->z, 1.0f);
    // v4          = CORRECTION.mul(v4);
    v->x = v4.x;
    v->y = v4.y;
    v->z = v4.z;
  }
  *positions = pos;
}

static void read_normals(XML_Node* mesh_node, Vector3** normals, u32& count)
{
  XML_Tag* source_tag = mesh_node->find_key("polylist")->find_key_with_tag("input", "semantic", "NORMAL")->find_tag("source");
  char*    normal_id  = (char*)sta_allocate_struct(char, source_tag->value_length);
  strncpy((char*)normal_id, &source_tag->value[1], source_tag->value_length - 1);
  normal_id[source_tag->value_length - 1] = '\0';

  XML_Node* normals_node                  = mesh_node->find_key_with_tag("source", "id", normal_id)->find_key("float_array");

  count                                   = sta_xml_get_count_from_tag(normals_node);
  Vector3* norm                           = (Vector3*)sta_allocate_struct(Vector3, count);
  Buffer   buffer                         = sta_xml_create_buffer_from_content(normals_node);

  buffer.parse_vector3_array(norm, count);
  // ToDo this needs to be tranformed since z up :)

  *normals = norm;
}

static void read_texture_coords(XML_Node* mesh_node, Vector2** uv, u32& count)
{
  XML_Tag* source_tag = mesh_node->find_key("polylist")->find_key_with_tag("input", "semantic", "TEXCOORD")->find_tag("source");
  char*    uv_id      = (char*)sta_allocate_struct(char, source_tag->value_length);
  strncpy((char*)uv_id, &source_tag->value[1], source_tag->value_length - 1);
  uv_id[source_tag->value_length - 1] = '\0';

  XML_Node* uv_node                   = mesh_node->find_key_with_tag("source", "id", uv_id)->find_key("float_array");
  count                               = sta_xml_get_count_from_tag(uv_node);
  Vector2* v                          = (Vector2*)sta_allocate_struct(Vector2, count);
  Buffer   buffer                     = sta_xml_create_buffer_from_content(uv_node);

  buffer.parse_vector2_array(v, count);

  *uv = v;
}

static void extract_model_data(MeshData* model, XML_Node* geometry, VertexSkinData* skin_data)
{

  XML_Node* mesh_node = geometry->find_key("geometry")->find_key("mesh");

  Vector3*  positions = {};
  u32       position_count;
  read_positions(mesh_node, &positions, position_count, skin_data);

  Vector3* normals = {};
  u32      normal_count;
  read_normals(mesh_node, &normals, normal_count);

  Vector2* uv = {};
  u32      uv_count;
  read_texture_coords(mesh_node, &uv, uv_count);

  XML_Node* poly       = mesh_node->find_key("polylist");
  int       faces      = sta_xml_get_count_from_tag(poly) * 3;
  int       type_count = sta_xml_get_child_count_by_name(poly, "input");

  XML_Node* p          = poly->find_key("p");

  Buffer    buffer     = sta_xml_create_buffer_from_content(p);

  model->vertex_count  = faces;
  model->indices       = (u32*)sta_allocate_struct(u32, faces);
  model->vertices      = (SkinnedVertex*)sta_allocate_struct(SkinnedVertex, faces);
  for (int i = 0; i < faces; i++)
  {

    buffer.skip_whitespace();
    u32 position_index = buffer.parse_int();

    buffer.skip_whitespace();
    u32 normal_index = buffer.parse_int();

    buffer.skip_whitespace();
    u32 uv_index = buffer.parse_int();

    for (u32 j = 0; j < type_count - 3; j++)
    {
      skip_whitespace(&buffer);
      (void)parse_int_from_string(&buffer);
    }

    model->indices[i]     = i;
    SkinnedVertex* vertex = &model->vertices[i];
    vertex->normal        = normals[normal_index];
    vertex->position      = positions[position_index];
    vertex->uv            = uv[uv_index];

    VertexSkinData* sd    = &skin_data[position_index];
    for (u32 j = 0; j < sd->count; j++)
    {
      vertex->joint_index[j]  = sd->data[j].joint_id;
      vertex->joint_weight[j] = sd->data[j].weight;
    }
    for (u32 j = sd->count; j < 4; j++)
    {
      vertex->joint_index[j]  = 0;
      vertex->joint_weight[j] = 0;
    }
  }
}

static void calc_inv_bind(JointData* joint, Mat44 parent_transform)
{
  Mat44 bind_transform          = parent_transform.mul(joint->local_bind_transform);
  joint->inverse_bind_transform = bind_transform.inverse();
  for (u32 i = 0; i < joint->children_count; i++)
  {
    calc_inv_bind(&joint->children[i], bind_transform);
  }
}

static inline void calculate_inverse_bind_transforms(SkeletonData* skeleton_data)
{
  Mat44 m = {};
  m.identity();
  calc_inv_bind(skeleton_data->head_joint, m);
}

static void load_collada_model(ColladaModelData* data, XML_Node* head)
{
  XML_Node* controller = head->find_key("library_controllers");
  extract_skin_data(&data->skinning_data, controller);

  XML_Node* geometry_node = head->find_key("library_geometries");
  extract_model_data(&data->mesh_data, geometry_node, data->skinning_data.vertex_skin_data);

  XML_Node* visual_scene = head->find_key("library_visual_scenes");
  extract_bone_data(&data->skeleton_data, visual_scene, &data->skinning_data);

  calculate_inverse_bind_transforms(&data->skeleton_data);
}

static void transfer_joints(Skeleton* skeleton, JointData* joint_data, u32 parent)
{
  Joint* joint         = &skeleton->joints[joint_data->index];
  joint->m_iParent     = parent;
  joint->m_mat         = joint_data->local_bind_transform;
  joint->m_invBindPose = joint_data->inverse_bind_transform;
  joint->m_name        = joint_data->name;
  joint->m_name_length = joint_data->name_length;

  printf("%.*s\n", (i32)joint->m_name_length, joint->m_name);
  joint->m_invBindPose.debug();
  printf("-\n");
  joint->m_mat.debug();

  for (u32 i = 0; i < joint_data->children_count; i++)
  {
    transfer_joints(skeleton, &joint_data->children[i], joint_data->index);
  }
}

static void find_root_joint_name(XML_Node* visual_scenes_node, String* root)
{
  XML_Node* skeleton = visual_scenes_node->find_key("visual_scene");
  XML_Tag*  node     = skeleton->find_key_with_tag("node", "id", "mixamorig_Hips")->find_tag("id");
  root->buffer       = (char*)node->value;
  root->length       = node->value_length;
}

static void get_key_times(XML_Node* node, f32** timesteps, u32& length)
{

  XML_Node* time_data = node->find_key("animation")->find_key("source")->find_key("float_array");

  Buffer    buffer    = sta_xml_create_buffer_from_content(time_data);

  length              = sta_xml_get_count_from_tag(time_data);
  f32* times          = (f32*)sta_allocate_struct(f32, length);
  buffer.parse_float_array(times, length);
  *timesteps = times;
}

static void get_joint_name(String* joint_name, XML_Node* joint_node)
{
  XML_Tag* tag = joint_node->find_key("channel")->find_tag("target");
  Buffer   buffer((char*)tag->value, tag->value_length);

  while (buffer.current_char() != '/')
  {
    buffer.advance();
  }

  joint_name->buffer = buffer.buffer;
  joint_name->length = buffer.index;
}

static void get_joint_id(String* joint_id, XML_Node* joint_node)
{
  XML_Tag* source_tag = joint_node->find_key("sampler")->find_key_with_tag("input", "semantic", "OUTPUT")->find_tag("source");
  joint_id->buffer    = &source_tag->value[1];
  joint_id->length    = source_tag->value_length - 1;
}

static void load_joint_transforms(Skeleton* skeleton, AnimationData* animation_data, XML_Node* joint_node)
{
  String joint_name = {};
  get_joint_name(&joint_name, joint_node);

  String data_id = {};
  get_joint_id(&data_id, joint_node);

  XML_Node* transform_data        = joint_node->find_key_with_tag("source", "id", data_id)->find_key("float_array");
  XML_Tag*  name_tag              = joint_node->find_tag("name");
  i32       joint_transform_index = get_joint_index_from_id(skeleton, name_tag->value, name_tag->value_length);

  Buffer    buffer                = sta_xml_create_buffer_from_content(transform_data);

  for (u32 i = 0; i < animation_data->count; i++)
  {
    KeyFrameData*       frame_data = &animation_data->key_frames[i];
    JointTransformData* transform  = &frame_data->transforms[joint_transform_index];

    buffer.parse_float_array(&transform->local_transform.m[0], 16);
    transform->local_transform.transpose();
  }
}

static void load_animation_data(Skeleton* skeleton, ColladaModelData* model_data, XML_Node* head)
{

  XML_Node*      animation_node = head->find_key("library_animations");
  AnimationData* animation_data = &model_data->animation_data;
  get_key_times(animation_node, &animation_data->timesteps, animation_data->count);
  animation_data->duration   = animation_data->timesteps[animation_data->count - 1];

  animation_data->key_frames = (KeyFrameData*)sta_allocate(sizeof(KeyFrameData) * animation_data->count);
  for (u32 i = 0; i < animation_data->count; i++)
  {
    animation_data->key_frames[i].time       = animation_data->timesteps[i];
    animation_data->key_frames[i].transforms = (JointTransformData*)sta_allocate(sizeof(JointTransformData) * model_data->skeleton_data.joint_count);
  }

  XML_Node* joint_node       = animation_node->child;
  u32       node_name_length = strlen("animation");

  while (joint_node)
  {
    if (joint_node->header.name_length == node_name_length && strncmp(joint_node->header.name, "animation", node_name_length) == 0)
    {
      load_joint_transforms(skeleton, animation_data, joint_node);
    }

    joint_node = joint_node->next;
  }
}

bool sta_collada_parse_from_file(AnimationModel* model, const char* filename)
{

  XML    xml    = {};
  Buffer buffer = {};
  if (!sta_read_file(&buffer, filename))
  {
    return 0;
  }

  if (!sta_xml_parse_version_and_encoding(&xml, &buffer))
  {
    return false;
  }

  if (!sta_xml_parse_from_buffer(&xml.head, &buffer))
  {
    return false;
  };

  ColladaModelData animation_model = {};
  load_collada_model(&animation_model, &xml.head);

  model->vertices             = animation_model.mesh_data.vertices;
  model->indices              = animation_model.mesh_data.indices;
  model->vertex_count         = animation_model.mesh_data.vertex_count;
  model->skeleton.joint_count = animation_model.skeleton_data.joint_count;
  model->skeleton.joints      = (Joint*)sta_allocate(sizeof(Joint) * model->skeleton.joint_count);

  transfer_joints(&model->skeleton, animation_model.skeleton_data.head_joint, 0);

  for (u32 i = 0; i < model->skeleton.joint_count; i++)
  {
    Joint* joint = &model->skeleton.joints[i];
  }

  load_animation_data(&model->skeleton, &animation_model, &xml.head);

  // model->animations.pose_count = animation_model.animation_data.count;
  // model->animations.duration   = animation_model.animation_data.duration;
  // model->animations.steps      = (f32*)sta_allocate(sizeof(f32) * model->animations.pose_count);
  // model->animations.poses      = (JointPose*)sta_allocate(sizeof(JointPose) * model->animations.pose_count);

  // // keyframe are how many different poses there are in a animation
  // // transforms are one for each joint
  // Animation* anim = &model->animations;
  // for (u32 i = 0; i < model->animations.pose_count; i++)
  // {
  //   JointPose* pose       = &model->animations.poses[i];
  //   pose->local_transform = (Mat44*)sta_allocate(sizeof(Mat44) * animation_model.skeleton_data.joint_count);
  //   anim->steps[i]        = animation_model.animation_data.timesteps[i];
  //   anim->joint_count     = animation_model.skeleton_data.joint_count;

  //   KeyFrameData* frame   = &animation_model.animation_data.key_frames[i];
  //   for (u32 j = 0; j < animation_model.skeleton_data.joint_count; j++)
  //   {

  //     pose->local_transform[j] = frame->transforms[j].local_transform;
  //   }
  // }
  // for (u32 i = 0; i < animation_model.skeleton_data.joint_count; i++)
  // {
  //   JointPose* pose = &model->animations.poses[0];
  //   if (pose->local_transform[i].rc[0][0] == 0 && pose->local_transform[i].rc[1][1] == 0)
  //   {
  //     Joint* joint = &model->skeleton.joints[i];
  //     printf("%.*s: %d\n", joint->m_name_length, joint->m_name, i);
  //   }
  // }

  return true;
}
