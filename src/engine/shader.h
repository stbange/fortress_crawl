#ifndef FC_SHADER_H
#define FC_SHADER_H

#include "engine/gl.h"

/* Compiles and links a program; prints the log and returns 0 on failure. */
GLuint shader_program(const char *name, const char *vs_src, const char *fs_src);

#endif
