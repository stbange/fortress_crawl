#include "render/renderer.h"

#include <SDL3/SDL.h>

#include "engine/shader.h"
#include "world/mesher.h"
#include "world/texgen.h"

/* ---------- shaders ---------- */

static const char *WORLD_VS =
    "#version 330 core\n"
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in uint a_tile;\n"
    "layout(location=3) in vec4 a_light;\n"
    "layout(location=4) in uvec2 a_nf;\n"
    "uniform mat4 u_vp;\n"
    "uniform vec3 u_origin;\n"
    "uniform float u_snap;\n"
    "uniform vec2 u_res;\n"
    "out vec3 v_wpos;\n"
    "out vec2 v_uv;\n"
    "flat out uint v_tile;\n"
    "out vec4 v_light;\n"
    "flat out uint v_n;\n"
    "flat out uint v_flags;\n"
    "void main() {\n"
    "  vec3 wp = u_origin + a_pos / 8.0;\n"
    "  v_wpos = wp; v_uv = a_uv; v_tile = a_tile; v_light = a_light; v_n = a_nf.x; v_flags = a_nf.y;\n"
    "  vec4 cp = u_vp * vec4(wp, 1.0);\n"
    "  if (u_snap > 0.5 && cp.w > 0.01) {\n"
    "    vec2 g = u_res * 0.5;\n"
    "    cp.xy = floor(cp.xy / cp.w * g + 0.5) / g * cp.w;\n"
    "  }\n"
    "  gl_Position = cp;\n"
    "}\n";

static const char *WORLD_FS =
    "#version 330 core\n"
    "in vec3 v_wpos;\n"
    "in vec2 v_uv;\n"
    "flat in uint v_tile;\n"
    "in vec4 v_light;\n"
    "flat in uint v_n;\n"
    "flat in uint v_flags;\n"
    "uniform sampler2D u_atlas;\n"
    "uniform vec3 u_cam;\n"
    "uniform vec3 u_fog;\n"
    "uniform float u_fog_density;\n"
    "uniform vec3 u_ambient;\n"
    "uniform vec3 u_torch;\n"
    "uniform float u_torch_radius;\n"
    "uniform float u_time;\n"
    "uniform float u_fullbright;\n"
    "uniform int u_ndl;\n"
    "uniform vec4 u_dl_pos[8];\n"
    "uniform vec3 u_dl_col[8];\n"
    "out vec4 frag;\n"
    "const vec3 N[7] = vec3[](vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1), vec3(0,1,0));\n"
    "void main() {\n"
    "  ivec2 tile = ivec2(int(v_tile % 64u), int(v_tile / 64u));\n"
    "  ivec2 t = ivec2(clamp(floor(v_uv), vec2(0.0), vec2(15.0)));\n"
    "  vec4 tex = texelFetch(u_atlas, tile * 16 + t, 0);\n"
    "  if (tex.a < 0.5) discard;\n"
    "  bool emissive = (v_flags & 1u) != 0u;\n"
    "  vec3 light = v_light.rgb;\n"
    "  float seed = dot(floor(v_wpos * 0.5), vec3(12.9898, 78.233, 37.719));\n"
    "  float fl = 0.84 + 0.09 * sin(u_time * 9.0 + seed) + 0.07 * sin(u_time * 23.0 + seed * 1.7);\n"
    "  light *= mix(1.0, fl, v_light.a);\n"
    "  vec3 L = u_cam - v_wpos;\n"
    "  float ld = length(L);\n"
    "  float att = clamp(1.0 - ld / u_torch_radius, 0.0, 1.0);\n"
    "  att *= att;\n"
    "  float ndl = (v_n == 6u) ? 0.8 : max(dot(N[min(v_n, 6u)], L / max(ld, 0.001)), 0.0) * 0.75 + 0.25;\n"
    "  float tfl = 0.92 + 0.08 * sin(u_time * 11.0) * sin(u_time * 7.3 + 1.0);\n"
    "  light += u_torch * att * ndl * tfl;\n"
    "  for (int i = 0; i < u_ndl; i++) {\n"
    "    vec3 dl = u_dl_pos[i].xyz - v_wpos;\n"
    "    float dd = length(dl);\n"
    "    float a = clamp(1.0 - dd / u_dl_pos[i].w, 0.0, 1.0);\n"
    "    float nl = (v_n == 6u) ? 0.8 : max(dot(N[min(v_n, 6u)], dl / max(dd, 0.001)), 0.0) * 0.8 + 0.2;\n"
    "    light += u_dl_col[i] * a * a * nl;\n"
    "  }\n"
    "  light = max(light, u_ambient);\n"
    "  if (emissive) light = vec3(max(v_light.r, 0.85)) * (0.92 + 0.08 * fl);\n"
    "  light = mix(light, vec3(1.0), u_fullbright);\n"
    "  light = floor(light * 14.0 + 0.5) / 14.0;\n"
    "  vec3 col = tex.rgb * light * 1.2;\n"
    "  float fog = 1.0 - exp(-pow(ld * u_fog_density, 1.5));\n"
    "  if (emissive) fog *= 0.55;\n"
    "  frag = vec4(mix(col, u_fog, clamp(fog, 0.0, 1.0)), 1.0);\n"
    "}\n";

static const char *FULLSCREEN_VS =
    "#version 330 core\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
    "  v_uv = p;\n"
    "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

static const char *SKY_FS =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "uniform mat4 u_inv_vp;\n"
    "uniform vec3 u_fog;\n"
    "uniform float u_stars;\n"
    "uniform float u_time;\n"
    "out vec4 frag;\n"
    "float hash(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }\n"
    "void main() {\n"
    "  vec4 a = u_inv_vp * vec4(v_uv * 2.0 - 1.0, -1.0, 1.0);\n"
    "  vec4 b = u_inv_vp * vec4(v_uv * 2.0 - 1.0, 1.0, 1.0);\n"
    "  vec3 dir = normalize(b.xyz / b.w - a.xyz / a.w);\n"
    "  vec3 col = u_fog;\n"
    "  if (u_stars > 0.0) {\n"
    "    vec3 cell = floor(dir * 160.0);\n"
    "    float h = hash(cell);\n"
    "    if (h > 0.9975) col += vec3(0.55, 0.55, 0.65) * u_stars * (0.6 + 0.4 * sin(u_time * 2.0 + h * 90.0)) * smoothstep(-0.05, 0.25, dir.y);\n"
    "    vec3 moon = normalize(vec3(0.35, 0.8, -0.45));\n"
    "    float m = dot(dir, moon);\n"
    "    if (m > 0.9985) col = mix(col, vec3(0.78, 0.8, 0.85), u_stars);\n"
    "    else col += vec3(0.07, 0.08, 0.1) * u_stars * pow(max(m, 0.0), 60.0);\n"
    "  }\n"
    "  frag = vec4(col, 1.0);\n"
    "}\n";

static const char *POST_FS =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform float u_dither;\n"
    "uniform vec2 u_low;\n"
    "out vec4 frag;\n"
    "const float bayer[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);\n"
    "void main() {\n"
    "  vec3 c = texture(u_tex, v_uv).rgb;\n"
    "  vec2 p = v_uv - 0.5;\n"
    "  c *= 1.0 - 0.55 * dot(p, p);\n"
    "  if (u_dither > 0.5) {\n"
    "    ivec2 px = ivec2(v_uv * u_low);\n"
    "    float b = (bayer[(px.y & 3) * 4 + (px.x & 3)] + 0.5) / 16.0 - 0.5;\n"
    "    float levels = 20.0;\n"
    "    c = floor(c * levels + 0.5 + b) / levels;\n"
    "  }\n"
    "  frag = vec4(c, 1.0);\n"
    "}\n";

/* ---------- setup ---------- */

static void create_fbo(Renderer *r) {
    if (r->fbo) {
        glDeleteFramebuffers(1, &r->fbo);
        glDeleteTextures(1, &r->fbo_color);
        glDeleteRenderbuffers(1, &r->fbo_depth);
    }
    r->fbo_w = r->set.low_w;
    r->fbo_h = r->set.low_h;
    glGenTextures(1, &r->fbo_color);
    glBindTexture(GL_TEXTURE_2D, r->fbo_color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, r->fbo_w, r->fbo_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &r->fbo_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, r->fbo_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, r->fbo_w, r->fbo_h);
    glGenFramebuffers(1, &r->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, r->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->fbo_color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, r->fbo_depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) SDL_Log("Low-res framebuffer incomplete");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool renderer_init(Renderer *r, int win_w, int win_h) {
    r->win_w = win_w;
    r->win_h = win_h;
    r->world_prog = shader_program("world", WORLD_VS, WORLD_FS);
    r->sky_prog = shader_program("sky", FULLSCREEN_VS, SKY_FS);
    r->post_prog = shader_program("post", FULLSCREEN_VS, POST_FS);
    if (!r->world_prog || !r->sky_prog || !r->post_prog) return false;
#define U(prog, name) glGetUniformLocation(prog, name)
    r->u_vp = U(r->world_prog, "u_vp");
    r->u_origin = U(r->world_prog, "u_origin");
    r->u_snap = U(r->world_prog, "u_snap");
    r->u_res = U(r->world_prog, "u_res");
    r->u_cam = U(r->world_prog, "u_cam");
    r->u_fog = U(r->world_prog, "u_fog");
    r->u_fog_density = U(r->world_prog, "u_fog_density");
    r->u_ambient = U(r->world_prog, "u_ambient");
    r->u_torch = U(r->world_prog, "u_torch");
    r->u_torch_radius = U(r->world_prog, "u_torch_radius");
    r->u_time = U(r->world_prog, "u_time");
    r->u_fullbright = U(r->world_prog, "u_fullbright");
    r->u_atlas = U(r->world_prog, "u_atlas");
    r->u_ndl = U(r->world_prog, "u_ndl");
    r->u_dl_pos = U(r->world_prog, "u_dl_pos");
    r->u_dl_col = U(r->world_prog, "u_dl_col");
    r->s_inv_vp = U(r->sky_prog, "u_inv_vp");
    r->s_fog = U(r->sky_prog, "u_fog");
    r->s_stars = U(r->sky_prog, "u_stars");
    r->s_time = U(r->sky_prog, "u_time");
    r->p_tex = U(r->post_prog, "u_tex");
    r->p_dither = U(r->post_prog, "u_dither");
    r->p_low = U(r->post_prog, "u_low");
#undef U

    /* texture atlas, filled lazily per theme */
    glGenTextures(1, &r->atlas);
    glBindTexture(GL_TEXTURE_2D, r->atlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    for (int i = 0; i < THEME_SLOTS; i++) r->slot_theme[i] = -1;

    /* one shared index buffer for all quads */
    u32 *idx = malloc(sizeof(u32) * 6 * MAX_QUADS_PER_DRAW);
    for (u32 q = 0; q < MAX_QUADS_PER_DRAW; q++) {
        u32 b = q * 4;
        idx[q * 6 + 0] = b; idx[q * 6 + 1] = b + 1; idx[q * 6 + 2] = b + 2;
        idx[q * 6 + 3] = b; idx[q * 6 + 4] = b + 2; idx[q * 6 + 5] = b + 3;
    }
    glGenBuffers(1, &r->quad_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->quad_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(sizeof(u32) * 6 * MAX_QUADS_PER_DRAW), idx, GL_STATIC_DRAW);
    free(idx);
    glGenVertexArrays(1, &r->empty_vao);
    create_fbo(r);
    return true;
}

void renderer_resize(Renderer *r, int win_w, int win_h) {
    r->win_w = win_w;
    r->win_h = win_h;
    if (r->fbo_w != r->set.low_w || r->fbo_h != r->set.low_h) create_fbo(r);
}

int renderer_theme_slot(Renderer *r, int theme) {
    for (int i = 0; i < THEME_SLOTS; i++)
        if (r->slot_theme[i] == theme) {
            r->slot_used_frame[i] = r->frame;
            return i;
        }
    /* free slot, else the least recently used one without meshes */
    int best = -1;
    for (int i = 0; i < THEME_SLOTS; i++) {
        if (r->slot_theme[i] < 0) { best = i; break; }
        if (r->slot_refs[i] == 0 && (best < 0 || r->slot_used_frame[i] < r->slot_used_frame[best])) best = i;
    }
    if (best < 0) best = 0; /* every slot in use: share (wrong textures until remeshed) */
    u8 *pix = malloc((size_t)TILES_PER_THEME * TEX_SIZE * TEX_SIZE * 4);
    texgen_theme(theme, pix);
    glBindTexture(GL_TEXTURE_2D, r->atlas);
    int base = best * TILES_PER_THEME;
    for (int t = 0; t < TILES_PER_THEME; t++) {
        int tile = base + t;
        glTexSubImage2D(GL_TEXTURE_2D, 0, (tile % ATLAS_TILES_PER_ROW) * 16, (tile / ATLAS_TILES_PER_ROW) * 16, 16, 16,
                        GL_RGBA, GL_UNSIGNED_BYTE, pix + (size_t)t * TEX_SIZE * TEX_SIZE * 4);
    }
    free(pix);
    r->slot_theme[best] = theme;
    r->slot_used_frame[best] = r->frame;
    return best;
}

void renderer_slot_ref(Renderer *r, int slot, int delta) {
    if (slot >= 0 && slot < THEME_SLOTS) r->slot_refs[slot] += delta;
}

void renderer_upload_mesh(Renderer *r, GLuint *vao, GLuint *vbo, const void *verts, int nquads) {
    if (!*vao) glGenVertexArrays(1, vao);
    if (!*vbo) glGenBuffers(1, vbo);
    glBindVertexArray(*vao);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(MeshVertex) * 4 * nquads, verts, GL_STATIC_DRAW);
    GLsizei st = (GLsizei)sizeof(MeshVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_SHORT, GL_FALSE, st, (void *)offsetof(MeshVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_UNSIGNED_BYTE, GL_FALSE, st, (void *)offsetof(MeshVertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_SHORT, st, (void *)offsetof(MeshVertex, tile));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void *)offsetof(MeshVertex, r));
    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 2, GL_UNSIGNED_BYTE, st, (void *)offsetof(MeshVertex, n));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->quad_ebo);
    glBindVertexArray(0);
}

void renderer_free_mesh(GLuint *vao, GLuint *vbo) {
    if (*vao) glDeleteVertexArrays(1, vao);
    if (*vbo) glDeleteBuffers(1, vbo);
    *vao = *vbo = 0;
}

/* ---------- frame ---------- */

static mat4 m4_inverse(mat4 m) {
    float *a = m.m, inv[16];
    inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
    inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
    inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
    inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
    inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
    inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
    inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
    inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
    inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
    inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
    inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
    inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
    inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
    inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
    inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
    inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
    float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    mat4 out;
    float id = fabsf(det) > 1e-12f ? 1.0f / det : 0.0f;
    for (int i = 0; i < 16; i++) out.m[i] = inv[i] * id;
    return out;
}

void renderer_frame_begin(Renderer *r) {
    r->frame++;
    if (r->fbo_w != r->set.low_w || r->fbo_h != r->set.low_h) create_fbo(r);
    glBindFramebuffer(GL_FRAMEBUFFER, r->fbo);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, r->fbo_w, r->fbo_h);
    glClearColor(0.02f, 0.015f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void renderer_set_view(Renderer *r, int x, int y, int w, int h) {
    /* x, y from the top-left like the UI, GL wants bottom-left */
    r->view_x = x;
    r->view_y = r->fbo_h - y - h;
    r->view_w = w;
    r->view_h = h;
    glViewport(r->view_x, r->view_y, w, h);
    glScissor(r->view_x, r->view_y, w, h);
    glEnable(GL_SCISSOR_TEST);
}

void renderer_begin(Renderer *r, const SceneParams *sp) {
    r->vp = m4_mul(sp->proj, sp->view);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    /* sky / background */
    glUseProgram(r->sky_prog);
    mat4 inv = m4_inverse(r->vp);
    glUniformMatrix4fv(r->s_inv_vp, 1, GL_FALSE, inv.m);
    glUniform3f(r->s_fog, sp->fog.x, sp->fog.y, sp->fog.z);
    glUniform1f(r->s_stars, sp->stars);
    glUniform1f(r->s_time, sp->time);
    glBindVertexArray(r->empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUseProgram(r->world_prog);
    glUniformMatrix4fv(r->u_vp, 1, GL_FALSE, r->vp.m);
    glUniform1f(r->u_snap, r->set.vertex_snap ? 1.0f : 0.0f);
    glUniform2f(r->u_res, (float)r->view_w, (float)r->view_h);
    glUniform3f(r->u_cam, sp->cam.x, sp->cam.y, sp->cam.z);
    glUniform3f(r->u_fog, sp->fog.x, sp->fog.y, sp->fog.z);
    glUniform1f(r->u_fog_density, sp->fog_density);
    glUniform3f(r->u_ambient, sp->ambient.x, sp->ambient.y, sp->ambient.z);
    if (r->set.torch) glUniform3f(r->u_torch, 1.05f, 0.72f, 0.42f);
    else glUniform3f(r->u_torch, 0, 0, 0);
    glUniform1f(r->u_torch_radius, 11.0f);
    glUniform1f(r->u_time, sp->time);
    glUniform1f(r->u_fullbright, r->set.fullbright ? 1.0f : 0.0f);
    {
        float pos[MAX_DLIGHTS * 4], col[MAX_DLIGHTS * 3];
        int n = MINI(sp->ndl, MAX_DLIGHTS);
        for (int i = 0; i < n; i++) {
            pos[i * 4 + 0] = sp->dl_pos[i].x; pos[i * 4 + 1] = sp->dl_pos[i].y; pos[i * 4 + 2] = sp->dl_pos[i].z;
            pos[i * 4 + 3] = sp->dl_rad[i];
            col[i * 3 + 0] = sp->dl_col[i].x; col[i * 3 + 1] = sp->dl_col[i].y; col[i * 3 + 2] = sp->dl_col[i].z;
        }
        glUniform1i(r->u_ndl, n);
        if (n > 0) {
            glUniform4fv(r->u_dl_pos, n, pos);
            glUniform3fv(r->u_dl_col, n, col);
        }
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->atlas);
    glUniform1i(r->u_atlas, 0);
}

void renderer_draw_chunk(Renderer *r, GLuint vao, int nsolid, int nsprite, vec3 origin) {
    glUniform3f(r->u_origin, origin.x, origin.y, origin.z);
    glBindVertexArray(vao);
    int ns = MINI(nsolid, MAX_QUADS_PER_DRAW);
    if (ns > 0) {
        glEnable(GL_CULL_FACE);
        glDrawElements(GL_TRIANGLES, ns * 6, GL_UNSIGNED_INT, NULL);
    }
    if (nsprite > 0 && nsolid + nsprite <= MAX_QUADS_PER_DRAW) {
        /* sprites are double sided; their quads follow the solid ones in the buffer */
        glDisable(GL_CULL_FACE);
        glDrawElements(GL_TRIANGLES, nsprite * 6, GL_UNSIGNED_INT, (void *)(sizeof(u32) * 6 * (size_t)nsolid));
        glEnable(GL_CULL_FACE);
    }
}

void renderer_end_world(Renderer *r) {
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void renderer_present(Renderer *r, float time) {
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    /* letterbox: keep the low-res aspect ratio */
    float sx = (float)r->win_w / r->fbo_w, sy = (float)r->win_h / r->fbo_h;
    float s = sx < sy ? sx : sy;
    int vw = (int)(r->fbo_w * s), vh = (int)(r->fbo_h * s);
    glViewport(0, 0, r->win_w, r->win_h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport((r->win_w - vw) / 2, (r->win_h - vh) / 2, vw, vh);
    glUseProgram(r->post_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->fbo_color);
    glUniform1i(r->p_tex, 0);
    glUniform1f(r->p_dither, r->set.dither ? 1.0f : 0.0f);
    glUniform2f(r->p_low, (float)r->fbo_w, (float)r->fbo_h);
    glBindVertexArray(r->empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

unsigned char *renderer_read_pixels(Renderer *r, int *w, int *h) {
    *w = r->win_w;
    *h = r->win_h;
    unsigned char *px = malloc((size_t)r->win_w * r->win_h * 3);
    unsigned char *row = malloc((size_t)r->win_w * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, r->win_w, r->win_h, GL_RGB, GL_UNSIGNED_BYTE, px);
    for (int y = 0; y < r->win_h / 2; y++) {
        unsigned char *a = px + (size_t)y * r->win_w * 3, *b = px + (size_t)(r->win_h - 1 - y) * r->win_w * 3;
        memcpy(row, a, (size_t)r->win_w * 3);
        memcpy(a, b, (size_t)r->win_w * 3);
        memcpy(b, row, (size_t)r->win_w * 3);
    }
    free(row);
    return px;
}
