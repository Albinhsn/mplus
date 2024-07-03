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
};

class Shader
{
public:
  unsigned int id;
  const char*  name;
  Shader()
  {
  }
  Shader(ShaderType* types, const char** file_locations, u32 count, const char* name);

  void use();

  void set_bool(const char* name, bool value);
  void set_int(const char* name, int value);
  void set_float(const char* name, float value);
  void set_float4f(const char* name, float value[4]);
  void set_mat4(const char* name, Mat44);
  void set_mat4(const char* name, float* v, int count);
  void set_mat4(const char* name, Mat44* v, int count);
};

#endif
