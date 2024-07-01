#ifndef SHADER_H
#define SHADER_H
#include "vector.h"

class Shader
{
public:
  unsigned int id;
  Shader(){}
  Shader(const char* vertex_path, const char* fragment_path);
  Shader(const char* vertex_path, const char * tessellation_control_path, const char * tessellation_evaluation_path, const char* fragment_path);

  void use();

  void set_bool(const char* name, bool value);
  void set_int(const char* name, int value);
  void set_float(const char* name, float value);
  void set_float4f(const char* name, float value[4]);
  void set_mat4(const char * name, Mat44);
  void set_mat4(const char * name, float * v, int count);
  void set_mat4(const char * name, Mat44* v, int count);
};

#endif
