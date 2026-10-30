#ifndef FC_GL_H
#define FC_GL_H

/* Minimal OpenGL 3.3 core loader built on SDL_GL_GetProcAddress.
 * SDL_opengl.h provides types and constants; every function we call goes
 * through a pointer named fcgl_<name>, and a macro maps gl<Name> onto it. */
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h> /* WebGL 2 is OpenGL ES 3.0 */
#else
#include <SDL3/SDL_opengl.h>
#endif
#include <stdbool.h>

#ifdef _WIN32
#define FC_APIENTRY __stdcall
#else
#define FC_APIENTRY
#endif

#define FC_GL_FUNCS(X) \
    X(void, Clear, (GLbitfield)) \
    X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, Viewport, (GLint, GLint, GLsizei, GLsizei)) \
    X(void, Enable, (GLenum)) \
    X(void, Disable, (GLenum)) \
    X(void, CullFace, (GLenum)) \
    X(void, DepthFunc, (GLenum)) \
    X(void, DepthMask, (GLboolean)) \
    X(void, BlendFunc, (GLenum, GLenum)) \
    X(const GLubyte *, GetString, (GLenum)) \
    X(void, GetIntegerv, (GLenum, GLint *)) \
    X(GLenum, GetError, (void)) \
    X(void, ReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)) \
    X(void, PixelStorei, (GLenum, GLint)) \
    X(void, GenTextures, (GLsizei, GLuint *)) \
    X(void, DeleteTextures, (GLsizei, const GLuint *)) \
    X(void, BindTexture, (GLenum, GLuint)) \
    X(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    X(void, TexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
    X(void, TexParameteri, (GLenum, GLenum, GLint)) \
    X(void, ActiveTexture, (GLenum)) \
    X(void, DrawElements, (GLenum, GLsizei, GLenum, const void *)) \
    X(void, DrawArrays, (GLenum, GLint, GLsizei)) \
    X(GLuint, CreateShader, (GLenum)) \
    X(void, ShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
    X(void, CompileShader, (GLuint)) \
    X(void, GetShaderiv, (GLuint, GLenum, GLint *)) \
    X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
    X(void, DeleteShader, (GLuint)) \
    X(GLuint, CreateProgram, (void)) \
    X(void, AttachShader, (GLuint, GLuint)) \
    X(void, LinkProgram, (GLuint)) \
    X(void, GetProgramiv, (GLuint, GLenum, GLint *)) \
    X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
    X(void, UseProgram, (GLuint)) \
    X(GLint, GetUniformLocation, (GLuint, const GLchar *)) \
    X(void, Uniform1i, (GLint, GLint)) \
    X(void, Uniform1f, (GLint, GLfloat)) \
    X(void, Uniform2f, (GLint, GLfloat, GLfloat)) \
    X(void, Uniform3f, (GLint, GLfloat, GLfloat, GLfloat)) \
    X(void, Uniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, Uniform3fv, (GLint, GLsizei, const GLfloat *)) \
    X(void, Uniform4fv, (GLint, GLsizei, const GLfloat *)) \
    X(void, UniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat *)) \
    X(void, GenBuffers, (GLsizei, GLuint *)) \
    X(void, DeleteBuffers, (GLsizei, const GLuint *)) \
    X(void, BindBuffer, (GLenum, GLuint)) \
    X(void, BufferData, (GLenum, GLsizeiptr, const void *, GLenum)) \
    X(void, GenVertexArrays, (GLsizei, GLuint *)) \
    X(void, DeleteVertexArrays, (GLsizei, const GLuint *)) \
    X(void, BindVertexArray, (GLuint)) \
    X(void, EnableVertexAttribArray, (GLuint)) \
    X(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
    X(void, VertexAttribIPointer, (GLuint, GLint, GLenum, GLsizei, const void *)) \
    X(void, GenFramebuffers, (GLsizei, GLuint *)) \
    X(void, DeleteFramebuffers, (GLsizei, const GLuint *)) \
    X(void, BindFramebuffer, (GLenum, GLuint)) \
    X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(GLenum, CheckFramebufferStatus, (GLenum)) \
    X(void, GenRenderbuffers, (GLsizei, GLuint *)) \
    X(void, DeleteRenderbuffers, (GLsizei, const GLuint *)) \
    X(void, BindRenderbuffer, (GLenum, GLuint)) \
    X(void, RenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei)) \
    X(void, FramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint))

#define FC_GL_DECLARE(ret, name, args) \
    typedef ret(FC_APIENTRY *fcgl_PFN_##name) args; \
    extern fcgl_PFN_##name fcgl_##name;
FC_GL_FUNCS(FC_GL_DECLARE)
#undef FC_GL_DECLARE

#define glClear fcgl_Clear
#define glClearColor fcgl_ClearColor
#define glViewport fcgl_Viewport
#define glEnable fcgl_Enable
#define glDisable fcgl_Disable
#define glCullFace fcgl_CullFace
#define glDepthFunc fcgl_DepthFunc
#define glDepthMask fcgl_DepthMask
#define glBlendFunc fcgl_BlendFunc
#define glGetString fcgl_GetString
#define glGetIntegerv fcgl_GetIntegerv
#define glGetError fcgl_GetError
#define glReadPixels fcgl_ReadPixels
#define glPixelStorei fcgl_PixelStorei
#define glGenTextures fcgl_GenTextures
#define glDeleteTextures fcgl_DeleteTextures
#define glBindTexture fcgl_BindTexture
#define glTexImage2D fcgl_TexImage2D
#define glTexSubImage2D fcgl_TexSubImage2D
#define glTexParameteri fcgl_TexParameteri
#define glActiveTexture fcgl_ActiveTexture
#define glDrawElements fcgl_DrawElements
#define glDrawArrays fcgl_DrawArrays
#define glCreateShader fcgl_CreateShader
#define glShaderSource fcgl_ShaderSource
#define glCompileShader fcgl_CompileShader
#define glGetShaderiv fcgl_GetShaderiv
#define glGetShaderInfoLog fcgl_GetShaderInfoLog
#define glDeleteShader fcgl_DeleteShader
#define glCreateProgram fcgl_CreateProgram
#define glAttachShader fcgl_AttachShader
#define glLinkProgram fcgl_LinkProgram
#define glGetProgramiv fcgl_GetProgramiv
#define glGetProgramInfoLog fcgl_GetProgramInfoLog
#define glUseProgram fcgl_UseProgram
#define glGetUniformLocation fcgl_GetUniformLocation
#define glUniform1i fcgl_Uniform1i
#define glUniform1f fcgl_Uniform1f
#define glUniform2f fcgl_Uniform2f
#define glUniform3f fcgl_Uniform3f
#define glUniform4f fcgl_Uniform4f
#define glUniform3fv fcgl_Uniform3fv
#define glUniform4fv fcgl_Uniform4fv
#define glUniformMatrix4fv fcgl_UniformMatrix4fv
#define glGenBuffers fcgl_GenBuffers
#define glDeleteBuffers fcgl_DeleteBuffers
#define glBindBuffer fcgl_BindBuffer
#define glBufferData fcgl_BufferData
#define glGenVertexArrays fcgl_GenVertexArrays
#define glDeleteVertexArrays fcgl_DeleteVertexArrays
#define glBindVertexArray fcgl_BindVertexArray
#define glEnableVertexAttribArray fcgl_EnableVertexAttribArray
#define glVertexAttribPointer fcgl_VertexAttribPointer
#define glVertexAttribIPointer fcgl_VertexAttribIPointer
#define glGenFramebuffers fcgl_GenFramebuffers
#define glDeleteFramebuffers fcgl_DeleteFramebuffers
#define glBindFramebuffer fcgl_BindFramebuffer
#define glFramebufferTexture2D fcgl_FramebufferTexture2D
#define glCheckFramebufferStatus fcgl_CheckFramebufferStatus
#define glGenRenderbuffers fcgl_GenRenderbuffers
#define glDeleteRenderbuffers fcgl_DeleteRenderbuffers
#define glBindRenderbuffer fcgl_BindRenderbuffer
#define glRenderbufferStorage fcgl_RenderbufferStorage
#define glFramebufferRenderbuffer fcgl_FramebufferRenderbuffer

/* Must be called after the GL context is current. Returns false if a function is missing. */
bool gl_load(void);

#endif
