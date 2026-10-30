#include "engine/shader.h"

#include <SDL3/SDL.h>
#include <string.h>

static GLuint compile(const char *name, GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
#ifdef __EMSCRIPTEN__
    /* Shaders are written for GLSL 3.30 core; GLSL ES 3.00 only needs a different header. */
    const char *core = "#version 330 core\n";
    if (strncmp(src, core, strlen(core)) == 0) src += strlen(core);
    const char *srcs[2] = { "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n", src };
    glShaderSource(s, 2, srcs, NULL);
#else
    glShaderSource(s, 1, &src, NULL);
#endif
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        SDL_Log("Shader '%s' (%s) failed to compile:\n%s", name,
                type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint shader_program(const char *name, const char *vs_src, const char *fs_src) {
    GLuint vs = compile(name, GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile(name, GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof log, NULL, log);
        SDL_Log("Shader '%s' failed to link:\n%s", name, log);
        return 0;
    }
    return p;
}
