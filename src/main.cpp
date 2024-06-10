#include "animation.h"
#include "files.h"
#include "platform.h"
#include "sdl.h"
#include "shader.h"
#include <GL/glext.h>
#include <SDL2/SDL_events.h>
#include <assimp/Importer.hpp>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <cassert>
#include <cfloat>
#include <cstdlib>

bool should_quit()
{
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    if (event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
    {
      return true;
    }
  }
  return false;
}
bool debug_me(ModelData* obj, AnimationModel* collada)
{
  const float EPSILON = 0.0001f;
  for (u64 i = 0; i < obj->vertex_count; i++)
  {
    u64 obj_index     = obj->indices[i];
    u64 collada_index = collada->indices[i];
    if (obj_index != collada_index)
    {
      printf("Wrong index @%ld, %ld vs %ld\n", i, obj_index, collada_index);
    }
    VertexData    obj_vertex     = obj->vertices[i];
    SkinnedVertex collada_vertex = collada->vertices[i];
    if (std::abs(obj_vertex.vertex.x - collada_vertex.position.x) > EPSILON)
    {
      printf("Obj: ");
      obj_vertex.vertex.debug();
      printf("Collada: ");
      collada_vertex.position.debug();
      printf("Failed x @%ld, %f vs %f\n", i, obj_vertex.vertex.x, collada_vertex.position.x);
      return false;
      // return false;
    }
    if (std::abs(obj_vertex.vertex.y - collada_vertex.position.y) > EPSILON)
    {
      printf("Failed y @%ld, %f vs %f\n", i, obj_vertex.vertex.y, collada_vertex.position.y);
      return false;
      // return false;
    }
    if (std::abs(obj_vertex.vertex.z - collada_vertex.position.z) > EPSILON)
    {
      printf("Failed normal x @%ld, %f vs %f\n", i, obj_vertex.vertex.x, collada_vertex.position.x);
      return false;
    }
  }
  return true;
}

void debug_joints(AnimationModel* model)
{
  Skeleton* skeleton = &model->skeleton;
  for (u32 i = 0; i < skeleton->joint_count; i++)
  {
    Joint* joint = &skeleton->joints[i];
    printf("%s -> %s\n", joint->m_name, skeleton->joints[joint->m_iParent].m_name);
  }
}

JointPose* find_joint_pose(Animation* animation, Joint* joint)
{
  for (unsigned int i = 0; i < animation->pose_count; i++)
  {

    JointPose* pose = &animation->poses[i];
    if (pose->name == 0)
    {
      continue;
    }
    if (strlen(pose->name) == strlen(joint->m_name) && strncmp(pose->name, joint->m_name, strlen(joint->m_name)) == 0)
    {
      return pose;
    }
  }
  return 0;
}

void debug_inv_bind(AnimationModel* animation)
{
  Skeleton* skeleton = &animation->skeleton;
  for (u32 i = 0; i < skeleton->joint_count; i++)
  {
    Joint joint = animation->skeleton.joints[i];
    printf("%d: \n", i);
    // joint.m_mat.debug();
    printf("-\n");
    joint.m_invBindPose.debug();
    printf("-----------\n");
  }
}

void debug_node(aiNode* node)
{
  printf("%s: \n", node->mName.C_Str());
  aiNode* parent = node->mParent;
  if (parent == 0)
  {
    printf("\tno parent\n");
  }
  else
  {
    printf("\tparent: %s\n", node->mParent->mName.C_Str());
  }
  printf("\ttrans:\n");
  for (int i = 0; i < 4; i++)
  {
    printf("\t\t");
    for (int j = 0; j < 4; j++)
    {
      printf("%f, ", node->mTransformation[i][j]);
    }
    printf("\n");
  }
  u32 children = node->mNumChildren;
  printf("Children: ");
  for (u32 i = 0; i < children; i++)
  {
    printf("%s, ", node->mChildren[i]->mName.C_Str());
  }
  printf("\n");
  for (u32 i = 0; i < children; i++)
  {
    debug_node(node->mChildren[i]);
  }
}

void debug_bone(aiBone* bone, int i)
{
  printf("%d: %s\n", i, bone->mName.C_Str());
  printf("\ttrans:\n");
  for (int i = 0; i < 4; i++)
  {
    printf("\t\t");
    for (int j = 0; j < 4; j++)
    {
      printf("%f, ", bone->mOffsetMatrix[i][j]);
    }
    printf("\n");
  }
}

void debug_mesh(aiMesh* mesh)
{
  printf("%s: \n", mesh->mName.C_Str());
  printf("bones:\n");
  for (u32 i = 0; i < mesh->mNumBones; i++)
  {
    debug_bone(mesh->mBones[i], i);
  }
  printf("Num Anim meshes %d\n", mesh->mNumAnimMeshes);
}

void create_animation_from_assimp_scene(AnimationModel* model, const aiScene* scene)
{

  // parse geometry
  aiMesh* mesh        = scene->mMeshes[0];
  model->vertex_count = mesh->mNumFaces * 3;
  model->vertices     = (SkinnedVertex*)sta_allocate(sizeof(SkinnedVertex) * model->vertex_count);
  model->indices      = (u32*)sta_allocate(sizeof(u32) * model->vertex_count);

  float low = FLT_MAX, high = -FLT_MAX;
  for (u32 i = 0; i < mesh->mNumFaces; i++)
  {

    aiFace* face = &mesh->mFaces[i];
    assert(face->mNumIndices == 3 && "How could this happen to me");
    for (u32 j = 0; j < face->mNumIndices; j++)
    {
      u32            idx  = i * 3 + j;
      SkinnedVertex* v    = &model->vertices[idx];
      model->indices[idx] = idx;

      aiVector3D* aiV     = &mesh->mVertices[face->mIndices[j]];
      v->position         = Vector3(aiV->x, aiV->y, aiV->z);

      low                 = MIN(v->position.x, low);
      low                 = MIN(v->position.y, low);
      low                 = MIN(v->position.z, low);

      high                = MAX(v->position.x, high);
      high                = MAX(v->position.y, high);
      high                = MAX(v->position.z, high);
      // v->position.debug();
    }
  }
  float diff = high - low;
  low        = low < 0 ? low : -low;
  for (u32 i = 0; i < model->vertex_count; i++)
  {
    Vector3* v = &model->vertices[i].position;
    v->x       = ((v->x - low) / diff) * 2.0f - 1.0f;
    v->y       = ((v->y - low) / diff) * 2.0f - 1.0f;
    v->z       = ((v->z - low) / diff) * 2.0f - 1.0f;
  }

  Skeleton* skeleton    = &model->skeleton;
  skeleton->joint_count = mesh->mNumBones;
  skeleton->joints      = (Joint*)sta_allocate(sizeof(Joint) * skeleton->joint_count);
  for (u32 i = 0; i < mesh->mNumBones; i++)
  {
    aiBone* bone  = mesh->mBones[i];
    Joint*  joint = &skeleton->joints[i];
    for (int row = 0; row < 4; row++)
    {
      for (int col = 0; col < 4; col++)
      {
        joint->m_invBindPose.rc[row][col] = bone->mOffsetMatrix[row][col];
      }
    }

    for (u32 j = 0; j < bone->mNumWeights; j++)
    {
      aiVertexWeight* weight = &bone->mWeights[j];
    }
  }

  aiAnimation* animation = scene->mAnimations[0];
  aiNodeAnim * channel = animation->mChannels[0];
}

int main()
{

  AnimationModel animation = {};
  // sta_collada_parse_from_file(&animation, "./data/unarmed_man/unarmed_opt.dae");
  Assimp::Importer importer;
  const aiScene*   scene = importer.ReadFile("./data/modelyup.dae", 0);
  if (scene == 0)
  {
    printf("%s\n", importer.GetErrorString());
    return 1;
  }

  create_animation_from_assimp_scene(&animation, scene);

  // return 1;
  // animation.debug();

  const int     screen_width  = 620;
  const int     screen_height = 480;

  SDL_GLContext context;
  SDL_Window*   window;
  sta_init_sdl_gl(&window, &context, screen_width, screen_height);

  ModelData model  = {};
  bool      result = sta_parse_wavefront_object_from_file(&model, "./data/unarmed_man/unarmed.obj");
  if (!result)
  {
    printf("Failed to read model!\n");
    return 1;
  }

  TargaImage image = {};
  result           = sta_read_targa_from_file_rgba(&image, "./data/unarmed_man/Peasant_Man_diffuse.tga");
  if (!result)
  {
    printf("Failed to read peasant man diffuse\n");
    return 1;
  }

  GLuint vao, vbo, ebo;
  sta_glGenVertexArrays(1, &vao);
  sta_glGenBuffers(1, &vbo);
  sta_glGenBuffers(1, &ebo);

  sta_glBindVertexArray(vao);
  sta_glBindBuffer(GL_ARRAY_BUFFER, vbo);
  sta_glBufferData(GL_ARRAY_BUFFER, sizeof(SkinnedVertex) * animation.vertex_count, animation.vertices, GL_STATIC_DRAW);

  sta_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  sta_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * animation.vertex_count, animation.indices, GL_STATIC_DRAW);

  int size = sizeof(float) * 16;
  sta_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, size, (void*)0);
  sta_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, size, (void*)(3 * sizeof(float)));
  sta_glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, size, (void*)(5 * sizeof(float)));
  sta_glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, size, (void*)(8 * sizeof(float)));
  sta_glVertexAttribIPointer(4, 4, GL_INT, size, (void*)(12 * sizeof(int)));
  sta_glEnableVertexAttribArray(0);
  sta_glEnableVertexAttribArray(1);
  sta_glEnableVertexAttribArray(2);
  sta_glEnableVertexAttribArray(3);
  sta_glEnableVertexAttribArray(4);

  unsigned int texture0;
  sta_glGenTextures(1, &texture0);
  sta_glBindTexture(GL_TEXTURE_2D, texture0);

  sta_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data);
  sta_glGenerateMipmap(GL_TEXTURE_2D);

  Shader shader("./shaders/animation.vert", "./shaders/animation.frag");

  shader.use();
  shader.set_int("texture1", 0);

  glActiveTexture(GL_TEXTURE0);
  sta_glBindTexture(GL_TEXTURE_2D, texture0);
  sta_glBindVertexArray(vao);

  u32   ticks = 0;

  Mat44 view  = {};
  view.identity();

  while (true)
  {

    if (should_quit())
    {
      break;
    }
    if (ticks + 16 < SDL_GetTicks())
    {
      ticks = SDL_GetTicks() + 16;
      view  = view.rotate_y(0.5f);
      shader.set_mat4("view", &view.m[0]);
    }
    sta_gl_clear_buffer(0.0f, 0.0f, 0.0f, 1.0f);

    glDrawElements(GL_TRIANGLES, animation.vertex_count, GL_UNSIGNED_INT, 0);
    sta_gl_render(window);
  }
}
