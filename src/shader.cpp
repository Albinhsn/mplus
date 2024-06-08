#include "shader.h"
#include "files.h"
#include "sdl.h"

static bool test_shader_compilation(unsigned int id) {
  int success;
  char info_log[512] = {};
  sta_glGetShaderiv(id, GL_COMPILE_STATUS, &success);

  if (!success) {
    sta_glGetShaderInfoLog(id, 512, NULL, info_log);
    printf("%s\n", info_log);
    return false;
  }

  return true;
}

static bool test_program_linking(unsigned int id) {
  int success;
  char info_log[512] = {};
  sta_glGetProgramiv(id, GL_LINK_STATUS, &success);

  if (!success) {
    sta_glGetProgramInfoLog(id, 512, NULL, info_log);
    printf("%s\n", info_log);
    return false;
  }

  return true;
}

void Shader::set_bool(const char *name, bool value) {
  sta_glUniform1i(sta_glGetUniformLocation(this->id, name), (int)value);
}

void Shader::set_float4f(const char *name, float f[4]) {
  sta_glUniform4fv(sta_glGetUniformLocation(this->id, name), 1, &f[0]);
}
void Shader::set_int(const char *name, int value) {
  sta_glUniform1i(sta_glGetUniformLocation(this->id, name), (int)value);
}

void Shader::set_float(const char *name, float value) {
  sta_glUniform1f(sta_glGetUniformLocation(this->id, name), value);
}

void Shader::set_mat4(const char *name, float *v) {

  sta_glUniformMatrix4fv(sta_glGetUniformLocation(this->id, name), 1, GL_FALSE,
                         v);
}
void Shader::set_mat4(const char *name, float *v, int count) {

  sta_glUniformMatrix4fv(sta_glGetUniformLocation(this->id, name), count, GL_FALSE,
                         v);
}

void Shader::use() { sta_glUseProgram(this->id); }
Shader::Shader(const char *vertex_path, const char *fragment_path) {

  Buffer buffer = {};
  sta_read_file(&buffer, vertex_path);
  const char *vertex_content = buffer.buffer;
  unsigned int vertex = sta_glCreateShader(GL_VERTEX_SHADER);
  sta_glShaderSource(vertex, 1, &vertex_content, 0);
  sta_glCompileShader(vertex);
  if (!test_shader_compilation(vertex)) {
    printf("Failed shader compilation!\n");
    return;
  }

  sta_read_file(&buffer, fragment_path);
  const char *fragment_content = buffer.buffer;
  unsigned int fragment = sta_glCreateShader(GL_FRAGMENT_SHADER);
  sta_glShaderSource(fragment, 1, &fragment_content, 0);
  sta_glCompileShader(fragment);
  if (!test_shader_compilation(fragment)) {
    printf("Failed shader compilation!\n");
    return;
  }

  this->id = sta_glCreateProgram();
  sta_glAttachShader(this->id, vertex);
  sta_glAttachShader(this->id, fragment);
  sta_glLinkProgram(this->id);

  test_program_linking(this->id);
}
