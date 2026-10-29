#ifndef FC_RENDERER_H
#define FC_RENDERER_H

#include "core/fmath.h"
#include "engine/gl.h"
#include "world/theme.h"

#define ATLAS_SIZE 1024
#define ATLAS_TILES_PER_ROW (ATLAS_SIZE / 16)
#define THEME_SLOTS 32
#define MAX_QUADS_PER_DRAW 250000
#define MAX_DLIGHTS 8

typedef struct RenderSettings {
    int low_w, low_h;   /* internal resolution */
    bool dither;
    bool vertex_snap;
    bool fullbright;
    bool torch;
    float fov_deg;
} RenderSettings;

/* Per-frame scene parameters (blended from the camera's surroundings). */
typedef struct SceneParams {
    mat4 view, proj;
    vec3 cam;
    vec3 fog;
    float fog_density;
    vec3 ambient;
    float stars;  /* 0..1 night sky visibility */
    float time;
    /* dynamic point lights (spells, explosions) */
    int ndl;
    vec3 dl_pos[MAX_DLIGHTS];
    vec3 dl_col[MAX_DLIGHTS];
    float dl_rad[MAX_DLIGHTS];
} SceneParams;

typedef struct Renderer {
    RenderSettings set;
    int win_w, win_h;
    GLuint fbo, fbo_color, fbo_depth;
    int fbo_w, fbo_h;
    GLuint world_prog, sky_prog, post_prog;
    GLuint atlas;
    int slot_theme[THEME_SLOTS];
    int slot_refs[THEME_SLOTS];
    unsigned slot_used_frame[THEME_SLOTS];
    unsigned frame;
    GLuint quad_ebo;
    GLuint empty_vao;
    /* uniform locations */
    GLint u_vp, u_origin, u_snap, u_res, u_cam, u_fog, u_fog_density, u_ambient, u_torch, u_torch_radius, u_time,
        u_fullbright, u_atlas, u_ndl, u_dl_pos, u_dl_col;
    GLint s_inv_vp, s_fog, s_stars, s_time;
    GLint p_tex, p_dither, p_low, p_time;
    mat4 vp;
} Renderer;

bool renderer_init(Renderer *r, int win_w, int win_h);
void renderer_resize(Renderer *r, int win_w, int win_h);
/* Atlas slot holding the textures of a theme; generates them on first use. */
int renderer_theme_slot(Renderer *r, int theme);
void renderer_slot_ref(Renderer *r, int slot, int delta);

/* Frame structure: begin -> draw chunks -> end_world -> (text into the low-res target) -> present. */
void renderer_begin(Renderer *r, const SceneParams *sp);
void renderer_draw_chunk(Renderer *r, GLuint vao, int nsolid, int nsprite, vec3 origin);
void renderer_end_world(Renderer *r);
void renderer_present(Renderer *r, float time);

/* Creates a VAO/VBO pair for a chunk mesh (vertices as produced by the mesher). */
void renderer_upload_mesh(Renderer *r, GLuint *vao, GLuint *vbo, const void *verts, int nquads);
void renderer_free_mesh(GLuint *vao, GLuint *vbo);

/* Reads the window back buffer as RGB rows, top row first. Caller frees. */
unsigned char *renderer_read_pixels(Renderer *r, int *w, int *h);

#endif
