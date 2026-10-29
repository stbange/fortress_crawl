#include "engine/gl.h"

#include <SDL3/SDL.h>

#define FC_GL_DEFINE(ret, name, args) fcgl_PFN_##name fcgl_##name;
FC_GL_FUNCS(FC_GL_DEFINE)
#undef FC_GL_DEFINE

bool gl_load(void) {
    bool ok = true;
#define FC_GL_LOAD(ret, name, args) \
    fcgl_##name = (fcgl_PFN_##name)SDL_GL_GetProcAddress("gl" #name); \
    if (!fcgl_##name) { SDL_Log("Missing GL function gl%s", #name); ok = false; }
    FC_GL_FUNCS(FC_GL_LOAD)
#undef FC_GL_LOAD
    return ok;
}
