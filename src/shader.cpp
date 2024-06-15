#include "shader.h"
#include "files.h"
#include "sdl.h"

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
  sta_glUniform1i(sta_glGetUniformLocation(this->id, name), (int)value);
}

void Shader::set_float4f(const char* name, float f[4])
{
  sta_glUniform4fv(sta_glGetUniformLocation(this->id, name), 1, &f[0]);
}
void Shader::set_int(const char* name, int value)
{
  sta_glUniform1i(sta_glGetUniformLocation(this->id, name), (int)value);
}

void Shader::set_float(const char* name, float value)
{
  sta_glUniform1f(sta_glGetUniformLocation(this->id, name), value);
}

void Shader::set_mat4(const char* name, float* v)
{
  this->set_mat4(name, v, 1);
}
void Shader::set_mat4(const char* name, float* v, int count)
{

  sta_glUniformMatrix4fv(sta_glGetUniformLocation(this->id, name), count, GL_FALSE, v);
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

Shader::Shader(const char* vertex_path, const char* tessellation_control_path, const char* tessellation_evaluation_path, const char* fragment_path)
{
  GLuint vertex = compile_shader(vertex_path, GL_VERTEX_SHADER);
  if (vertex == -1)
  {
    return;
  }

  GLuint fragment = compile_shader(fragment_path, GL_FRAGMENT_SHADER);
  if (fragment == -1)
  {
    return;
  }
  GLuint tessellation_control = compile_shader(tessellation_control_path, GL_TESS_CONTROL_SHADER);
  if (fragment == -1)
  {
    return;
  }
  GLuint tessellation_evaluation = compile_shader(tessellation_evaluation_path, GL_TESS_EVALUATION_SHADER);
  if (fragment == -1)
  {
    return;
  }

  this->id = sta_glCreateProgram();
  sta_glAttachShader(this->id, vertex);
  sta_glAttachShader(this->id, fragment);
  sta_glAttachShader(this->id, tessellation_control);
  sta_glAttachShader(this->id, tessellation_evaluation);
  sta_glLinkProgram(this->id);

  test_program_linking(this->id);
}

Shader::Shader(const char* vertex_path, const char* fragment_path)
{

  GLuint vertex = compile_shader(vertex_path, GL_VERTEX_SHADER);
  if (vertex == -1)
  {
    return;
  }

  GLuint fragment = compile_shader(fragment_path, GL_FRAGMENT_SHADER);
  if (fragment == -1)
  {
    return;
  }

  this->id = sta_glCreateProgram();
  sta_glAttachShader(this->id, vertex);
  sta_glAttachShader(this->id, fragment);
  sta_glLinkProgram(this->id);

  test_program_linking(this->id);
}
