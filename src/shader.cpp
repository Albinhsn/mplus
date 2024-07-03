#include "shader.h"
#include <GL/glext.h>

static bool test_shader_compilation(unsigned int id)
{
  int  success;
  char info_log[512] = {};
  sta_glGetShaderiv(id, GL_COMPILE_STATUS, &success);

  if (!success)
  {
    sta_glGetShaderInfoLog(id, 512, NULL, info_log);
    printf("%s\n", info_log);
    return false;
  }

  return true;
}

static GLint get_location(GLuint id, const char* name, const char* shader_name)
{

  GLint location = sta_glGetUniformLocation(id, name);
  if (location == -1)
  {
    logger.error("Couldn't find uniform '%s' in shader '%s'", name, shader_name);
    assert(!"Couldn't find uniform!");
  }
  return location;
}

static bool test_program_linking(unsigned int id)
{
  int  success;
  char info_log[512] = {};
  sta_glGetProgramiv(id, GL_LINK_STATUS, &success);

  if (!success)
  {
    sta_glGetProgramInfoLog(id, 512, NULL, info_log);
    printf("%s\n", info_log);
    return false;
  }

  return true;
}

void Shader::set_bool(const char* name, bool value)
{
  sta_glUniform1i(get_location(this->id, name, this->name), (int)value);
}

void Shader::set_float4f(const char* name, float f[4])
{
  sta_glUniform4fv(get_location(this->id, name, this->name), 1, &f[0]);
}
void Shader::set_int(const char* name, int value)
{
  sta_glUniform1i(get_location(this->id, name, this->name), (int)value);
}

void Shader::set_float(const char* name, float value)
{
  sta_glUniform1f(get_location(this->id, name, this->name), value);
}

void Shader::set_mat4(const char* name, Mat44 m)
{
  this->set_mat4(name, &m.m[0], 1);
}
void Shader::set_mat4(const char* name, Mat44* v, int count)
{
  this->set_mat4(name, (float*)&v[0].m[0], count);
}

void Shader::set_mat4(const char* name, float* v, int count)
{

  sta_glUniformMatrix4fv(get_location(this->id, name, this->name), count, GL_FALSE, v);
}

void Shader::use()
{
  sta_glUseProgram(this->id);
}

static GLuint compile_shader(const char* path, GLint shader_type)
{

  Buffer buffer = {};
  sta_read_file(&buffer, path);
  const char*  vertex_content = buffer.buffer;
  unsigned int vertex         = sta_glCreateShader(shader_type);
  sta_glShaderSource(vertex, 1, &vertex_content, 0);
  sta_glCompileShader(vertex);
  if (!test_shader_compilation(vertex))
  {
    printf("Failed shader compilation!\n");
    return -1;
  }
  return vertex;
}

static inline GLuint get_gl_shader_type(ShaderType type)
{
  switch (type)
  {
  case SHADER_TYPE_VERT:
  {
    return GL_VERTEX_SHADER;
  }
  case SHADER_TYPE_FRAG:
  {
    return GL_FRAGMENT_SHADER;
  }
  case SHADER_TYPE_TESS_CONTROL:
  {
    return GL_TESS_CONTROL_SHADER;
  }
  case SHADER_TYPE_TESS_EVALUATION:
  {
    return GL_TESS_EVALUATION_SHADER;
  }
  case SHADER_TYPE_COMPUTE:
  {
    return GL_COMPUTE_SHADER;
  }
  }
  assert(!"Unknown shader type to gl conversion?");
}

Shader::Shader(ShaderType* types, const char** names, u32 count, const char* name)
{
  this->name = name;
  this->id   = sta_glCreateProgram();

  for (u32 i = 0; i < count; i++)
  {

    GLuint shader_id = compile_shader(names[i], get_gl_shader_type(types[i]));
    if ((i32)shader_id == -1)
    {
      logger.error("Failed to compile shader '%s'", names[i]);
      assert(!"Failed to compile shader!");
      return;
    }
    sta_glAttachShader(this->id, shader_id);
  }
  sta_glLinkProgram(this->id);

  test_program_linking(this->id);
}
