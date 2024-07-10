#ifndef STA_LIB_SDL_H
#define STA_LIB_SDL_H
#include "common.h"
#include "files.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL.h>

typedef GLuint(APIENTRY* PFNGLCREATESHADERPROC)(GLenum type);
typedef void(APIENTRY* PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void(APIENTRY* PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);
typedef void(APIENTRY* PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei* length, char* infoLog);
typedef GLuint(APIENTRY* PFNGLCREATEPROGRAMPROC)(void);
typedef void(APIENTRY* PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void(APIENTRY* PFNGLBINDATTRIBLOCATIONPROC)(GLuint program, GLuint index, const char* name);
typedef void(APIENTRY* PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void(APIENTRY* PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint* params);
typedef void(APIENTRY* PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei* length, char* infoLog);
typedef void(APIENTRY* PFNGLDETACHSHADERPROC)(GLuint program, GLuint shader);
typedef void(APIENTRY* PFNGLDELETESHADERPROC)(GLuint shader);
typedef void(APIENTRY* PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void(APIENTRY* PFNGLUSEPROGRAMPROC)(GLuint program);
typedef GLint(APIENTRY* PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const char* name);
typedef void(APIENTRY* PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void(APIENTRY* PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void(APIENTRY* PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void(APIENTRY* PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void(APIENTRY* PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void(APIENTRY* PFNGLBUFFERDATAPROC)(GLenum target, ptrdiff_t size, const GLvoid* data, GLenum usage);
typedef void(APIENTRY* PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void(APIENTRY* PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);
typedef void(APIENTRY* PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void(APIENTRY* PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
typedef void(APIENTRY* PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
typedef void(APIENTRY* PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void(APIENTRY* PFNGLGENERATEMIPMAPPROC)(GLenum target);
typedef void(APIENTRY* PFNGLUNIFORM2FVPROC)(GLint location, GLsizei count, const GLfloat* value);
typedef void(APIENTRY* PFNGLUNIFORM3FVPROC)(GLint location, GLsizei count, const GLfloat* value);
typedef void(APIENTRY* PFNGLUNIFORM4FVPROC)(GLint location, GLsizei count, const GLfloat* value);
typedef void*(APIENTRY* PFNGLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef void*(APIENTRY* PFNGLBUFFERSUBDATA)(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);
typedef GLboolean(APIENTRY* PFNGLUNMAPBUFFERPROC)(GLenum target);
typedef void*(APIENTRY* PFNGLBUFFERSTORAGE)(GLenum target, GLsizeiptr size, const GLvoid* data, GLbitfield flags);
typedef void*(APIENTRY* PFNGLNAMEDBUFFERSTORAGE)(GLenum target, GLsizeiptr size, const GLvoid* data, GLbitfield flags);
typedef void(APIENTRY* PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void(APIENTRY* PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint* framebuffers);
typedef void(APIENTRY* PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint* framebuffers);
typedef void(APIENTRY* PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void(APIENTRY* PFNGLGENRENDERBUFFERSPROC)(GLsizei n, GLuint* renderbuffers);
typedef void(APIENTRY* PFNGLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
typedef void(APIENTRY* PFNGLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void(APIENTRY* PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void(APIENTRY* PFNGLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint* renderbuffers);
typedef void(APIENTRY* PFNGLBLENDFUNCSEPARATEPROC)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void(APIENTRY* PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const char* const* string, const GLint* length);
typedef void(APIENTRY* PFNGLGENERATEMIPMAPPROC)(GLenum target);
typedef void*(APIENTRY* PFNGLMAPNAMEDBUFFERPROC)(GLuint buffer, GLenum access);
typedef void(APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void(APIENTRY* PFNGLDRAWBUFFERSPROC)(GLint n, const GLenum* bufs);
typedef void(APIENTRY* PFNGLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint* renderbuffers);
typedef void(APIENTRY* PFNGLGENERATEMIPMAPPROC)(GLuint n);
typedef void(APIENTRY* PFNGLCREATEVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void(APIENTRY* PFNGLCREATEBUFFERSPROC)(GLsizei n, GLuint* arrays);
typedef void(APIENTRY* PFNGLVERTEXARRAYVERTEXBUFFERPROC)(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
typedef void(APIENTRY* PFNGLENABLEVERTEXARRAYATTRIBPROC)(GLuint vaobj, GLuint index);
typedef void(APIENTRY* PFNGLVERTEXARRAYATTRIBFORMATPROC)(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
typedef void(APIENTRY* PFNGLVERTEXARRAYATTRIBBINDINGPROC)(GLuint vaobj, GLuint attribindex, GLuint bindingindex);
typedef void(APIENTRY* PFNGLFRAMEBUFFERTEXTUREPROC)(GLenum target, GLenum attachment, GLuint texture, GLint level);


inline void sta_gl_clear_buffer(f32 r, f32 g, f32 b, f32 a)
{
  glClearColor(r, g, b, a);
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}
void      sta_glCreateVertexArrays(GLsizei n, GLuint* arrays);
void      sta_glCreateBuffers(GLsizei n, GLuint* arrays);
void      sta_glVertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
void      sta_glEnableVertexArrayAttrib(GLuint vaobj, GLuint index);
void      sta_glVertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
void      sta_glVertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex);

void      sta_glNamedBufferStorage(GLenum target, GLsizeiptr size, const GLvoid* data, GLbitfield flags);
void      sta_glBufferStorage(GLenum target, GLsizeiptr size, const GLvoid* data, GLbitfield flags);
void      sta_glNamedBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);
void      sta_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);
void      sta_updateWindowSizeSDL(SDL_Window* window, i32 width, i32 height);
void      sta_gl_draw_lines(GLuint vertex_array, u32 vertex_count, u32 width, Color color);
void      sta_gl_render(SDL_Window* window);
void      sta_init_sdl_gl(SDL_Window** window, SDL_GLContext* context, int screenWidth, int screenHeight, bool vsync);
void      sta_init_sdl_window(u8** buffer, SDL_Window** window, u64 screenWidth, u64 screenHeight);
GLuint    sta_glCreateShader(GLenum type);
void      sta_glCompileShader(GLuint shader);
void      sta_glGetShaderiv(GLuint shader, GLenum pname, GLint* params);
void      sta_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, char* infoLog);
GLuint    sta_glCreateProgram();
void      sta_glAttachShader(GLuint program, GLuint shader);
void      sta_glBindAttribLocation(GLuint program, GLuint index, const char* name);
void      sta_glLinkProgram(GLuint program);
void      sta_glGetProgramiv(GLuint program, GLenum pname, GLint* params);
void      sta_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, char* infoLog);
void      sta_glDetachShader(GLuint program, GLuint shader);
void      sta_glDeleteProgram(GLuint program);
void      sta_glUseProgram(GLuint program);
GLint     sta_glGetUniformLocation(GLuint program, const char* name);
void      sta_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void      sta_glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void      sta_glGenVertexArrays(GLsizei n, GLuint* arrays);
void      sta_glGenTextures(GLsizei n, GLuint* textures);
void      sta_glBindTexture(GLenum target, GLuint texture);
void      sta_glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels);
void      sta_glBindVertexArray(GLuint array);
void      sta_glGenBuffers(GLsizei n, GLuint* buffers);
void      sta_glBindBuffer(GLenum target, GLuint buffer);
void      sta_glBufferData(GLenum target, ptrdiff_t size, const GLvoid* data, GLenum usage);
void      sta_glEnableVertexAttribArray(GLuint index);
void      sta_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);
void      sta_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void      sta_glDisableVertexAttribArray(GLuint index);
void      sta_glDeleteBuffers(GLsizei n, const GLuint* buffers);
void      sta_glDeleteVertexArrays(GLsizei n, const GLuint* arrays);
void      sta_glUniform1i(GLint location, GLint v0);
void      sta_glGenerateMipmap(GLenum target);
void      sta_glUniform2fv(GLint location, GLsizei count, const GLfloat* value);
void      sta_glUniform3fv(GLint location, GLsizei count, const GLfloat* value);
void      sta_glUniform4fv(GLint location, GLsizei count, const GLfloat* value);
void*     sta_glMapBuffer(GLenum target, GLenum access);
GLboolean sta_glUnmapBuffer(GLenum target);
void      sta_glUniform1f(GLint location, GLfloat v0);
void      sta_glGenFramebuffers(GLsizei n, GLuint* framebuffers);
void      sta_glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
void      sta_glBindFramebuffer(GLenum target, GLuint framebuffer);
void      sta_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void      sta_glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level);
void      sta_glGenRenderbuffers(GLsizei n, GLuint* renderbuffers);
void      sta_glBindRenderbuffer(GLenum target, GLuint renderbuffer);
void      sta_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
void      sta_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
void      sta_glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
void      sta_glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
void      sta_glShaderSource(GLuint shader, GLsizei count, const char* const* string, const GLint* length);
void      sta_glMapNamedBuffer(GLuint buffer, GLenum access);
void      sta_glDrawBuffers(GLint n, const GLenum* bufs);

#endif
