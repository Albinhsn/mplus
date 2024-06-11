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
struct JointAndIdx
{
  Joint* joint;
  u8     idx;
};

JointAndIdx get_joint_from_node(Skeleton* skeleton, aiNode* node)
{
  for (u32 i = 0; i < skeleton->joint_count; i++)
  {
    Joint* joint = &skeleton->joints[i];
    printf("Looking at %.*s vs %.*s\n", (i32)strlen(joint->m_name), joint->m_name, (i32)strlen(node->mName.data), node->mName.data);
    if (strlen(joint->m_name) == strlen(node->mName.data) && strncmp(joint->m_name, node->mName.data, strlen(joint->m_name)))
    {
      struct JointAndIdx idx = {joint, (u8)i};
      return idx;
    }
  }
  return {0, 0};
}


int main()
{

  AnimationModel animation = {};
  sta_collada_parse_from_file(&animation, "./data/model.dae");

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
