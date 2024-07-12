#ifndef SHADER_H
#define SHADER_H
#include "vector.h"
#include "sdl.h"

enum ShaderType
{
  SHADER_TYPE_VERT,
  SHADER_TYPE_FRAG,
  SHADER_TYPE_TESS_CONTROL,
  SHADER_TYPE_TESS_EVALUATION,
  SHADER_TYPE_COMPUTE,
  SHADER_TYPE_GEOMETRY,
  SHADER_TYPE_COUNT
};

class Shader
{
public:
  unsigned int id;
  const char*  name;
  const char*  locations[SHADER_TYPE_COUNT];
  ShaderType   types[SHADER_TYPE_COUNT];
  GLuint shader_ids[SHADER_TYPE_COUNT];

  Shader()
  {
  }
  Shader(ShaderType* types, const char** file_locations, u32 count, const char* name);

  void use();
  void set_vec3(const char* name, Vector3 v);
  void set_bool(const char* name, bool value);
  void set_int(const char* name, int value);
  void set_float(const char* name, float value);
  void set_float4f(const char* name, float value[4]);
  void set_mat4(const char* name, Mat44);
  void set_mat4(const char* name, float* v, int count);
  void set_mat4(const char* name, Mat44* v, int count);
};
bool recompile_shader(Shader* shader);

#endif
