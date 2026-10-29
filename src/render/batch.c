#include "render/batch.h"

#include "engine/gl.h"
#include "engine/shader.h"

#define MAX_V3 (6 * 6 * 4096)
#define MAX_V2 (6 * 16384)

static GLuint g_prog3, g_vao3, g_vbo3, g_prog2, g_vao2, g_vbo2;
static GLint u3_vp, u3_cam, u3_fog, u3_density;
static float *g_v3, *g_v2;
static int g_n3, g_n2, g_w2, g_h2;

static const char *VS3 =
    "#version 330 core\n"
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec4 a_col;\n"
    "uniform mat4 u_vp;\n"
    "out vec3 v_wpos; out vec4 v_col;\n"
    "void main() { v_wpos = a_pos; v_col = a_col; gl_Position = u_vp * vec4(a_pos, 1.0); }\n";
static const char *FS3 =
    "#version 330 core\n"
    "in vec3 v_wpos; in vec4 v_col;\n"
    "uniform vec3 u_cam; uniform vec3 u_fog; uniform float u_density;\n"
    "out vec4 frag;\n"
    "void main() {\n"
    "  vec3 c = floor(v_col.rgb * 14.0 + 0.5) / 14.0;\n"
    "  float d = length(v_wpos - u_cam);\n"
    "  float f = 1.0 - exp(-pow(d * u_density, 1.5));\n"
    "  if (v_col.a > 0.5) f *= 0.5;\n"
    "  frag = vec4(mix(c, u_fog, clamp(f, 0.0, 1.0)), 1.0);\n"
    "}\n";
static const char *VS2 =
    "#version 330 core\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec4 a_col;\n"
    "out vec4 v_col;\n"
    "void main() { v_col = a_col; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
static const char *FS2 =
    "#version 330 core\n"
    "in vec4 v_col; out vec4 frag;\n"
    "void main() { frag = v_col; }\n";

static void setup_vao(GLuint *vao, GLuint *vbo, int pos_comps) {
    int stride = (pos_comps + 4) * (int)sizeof(float);
    glGenVertexArrays(1, vao);
    glGenBuffers(1, vbo);
    glBindVertexArray(*vao);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, pos_comps, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void *)(sizeof(float) * (size_t)pos_comps));
    glBindVertexArray(0);
}

void batch_init(void) {
    g_prog3 = shader_program("batch3d", VS3, FS3);
    g_prog2 = shader_program("batch2d", VS2, FS2);
    u3_vp = glGetUniformLocation(g_prog3, "u_vp");
    u3_cam = glGetUniformLocation(g_prog3, "u_cam");
    u3_fog = glGetUniformLocation(g_prog3, "u_fog");
    u3_density = glGetUniformLocation(g_prog3, "u_density");
    setup_vao(&g_vao3, &g_vbo3, 3);
    setup_vao(&g_vao2, &g_vbo2, 2);
    g_v3 = malloc(sizeof(float) * 7 * MAX_V3);
    g_v2 = malloc(sizeof(float) * 6 * MAX_V2);
}

/* ---------- 3D ---------- */

void batch3d_begin(void) { g_n3 = 0; }

static void v3push(vec3 p, vec3 c, bool e) {
    if (g_n3 >= MAX_V3) return;
    float *q = g_v3 + (size_t)g_n3 * 7;
    q[0] = p.x; q[1] = p.y; q[2] = p.z;
    q[3] = c.x; q[4] = c.y; q[5] = c.z; q[6] = e ? 1.0f : 0.0f;
    g_n3++;
}

/* Simple fixed lighting by face normal, like the world's face shading. */
static float face_shade(vec3 n) {
    if (n.y > 0.5f) return 1.0f;
    if (n.y < -0.5f) return 0.55f;
    return 0.72f + 0.12f * fabsf(n.x);
}

static void tri(vec3 a, vec3 b, vec3 c, vec3 col, bool e) {
    vec3 n = v3_norm(v3_cross(v3_sub(b, a), v3_sub(c, a)));
    vec3 lc = e ? col : v3_scale(col, face_shade(n));
    v3push(a, lc, e); v3push(b, lc, e); v3push(c, lc, e);
}

static void quad(vec3 a, vec3 b, vec3 c, vec3 d, vec3 col, bool e) {
    tri(a, b, c, col, e);
    tri(a, c, d, col, e);
}

void batch3d_obox(vec3 c, vec3 ax, vec3 ay, vec3 az, vec3 col, bool e) {
    vec3 p[8];
    for (int i = 0; i < 8; i++) {
        vec3 v = c;
        v = v3_add(v, v3_scale(ax, (i & 1) ? 1.0f : -1.0f));
        v = v3_add(v, v3_scale(ay, (i & 2) ? 1.0f : -1.0f));
        v = v3_add(v, v3_scale(az, (i & 4) ? 1.0f : -1.0f));
        p[i] = v;
    }
    /* counter-clockwise from outside */
    quad(p[1], p[3], p[7], p[5], col, e); /* +x */
    quad(p[0], p[4], p[6], p[2], col, e); /* -x */
    quad(p[2], p[6], p[7], p[3], col, e); /* +y */
    quad(p[0], p[1], p[5], p[4], col, e); /* -y */
    quad(p[4], p[5], p[7], p[6], col, e); /* +z */
    quad(p[0], p[2], p[3], p[1], col, e); /* -z */
}

void batch3d_box(vec3 c, vec3 he, float yaw, vec3 col, bool e) {
    float cs = cosf(yaw), sn = sinf(yaw);
    batch3d_obox(c, v3(cs * he.x, 0, sn * he.x), v3(0, he.y, 0), v3(-sn * he.z, 0, cs * he.z), col, e);
}

void batch3d_octa(vec3 c, float r, float h, float yaw, vec3 col, bool e) {
    vec3 top = v3_add(c, v3(0, h, 0)), bot = v3_add(c, v3(0, -h, 0));
    vec3 ring[4];
    for (int i = 0; i < 4; i++) {
        float a = yaw + i * PI_F * 0.5f;
        ring[i] = v3_add(c, v3(cosf(a) * r, 0, sinf(a) * r));
    }
    for (int i = 0; i < 4; i++) {
        vec3 a = ring[i], b = ring[(i + 1) % 4];
        tri(top, b, a, col, e);
        tri(bot, a, b, col, e);
    }
}

void batch3d_spike(vec3 c, vec3 dir, float r, float h, vec3 col, bool e) {
    vec3 d = v3_norm(dir);
    vec3 up = fabsf(d.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0);
    vec3 s = v3_norm(v3_cross(d, up)), t = v3_cross(s, d);
    vec3 apex = v3_add(c, v3_scale(d, h));
    vec3 q[4] = { v3_add(c, v3_add(v3_scale(s, r), v3_scale(t, r))), v3_add(c, v3_add(v3_scale(s, -r), v3_scale(t, r))),
                  v3_add(c, v3_add(v3_scale(s, -r), v3_scale(t, -r))), v3_add(c, v3_add(v3_scale(s, r), v3_scale(t, -r))) };
    for (int i = 0; i < 4; i++) {
        tri(apex, q[i], q[(i + 1) % 4], col, e);
        tri(apex, q[(i + 1) % 4], q[i], col, e);
    }
    quad(q[0], q[3], q[2], q[1], col, e);
}

void batch3d_flush(const mat4 *vp, const BatchFog *fog) {
    if (!g_n3) return;
    glUseProgram(g_prog3);
    glUniformMatrix4fv(u3_vp, 1, GL_FALSE, vp->m);
    glUniform3f(u3_cam, fog->cam.x, fog->cam.y, fog->cam.z);
    glUniform3f(u3_fog, fog->color.x, fog->color.y, fog->color.z);
    glUniform1f(u3_density, fog->density);
    glBindVertexArray(g_vao3);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo3);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(float) * 7 * (size_t)g_n3), g_v3, GL_STREAM_DRAW);
    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLES, 0, g_n3);
    glBindVertexArray(0);
    g_n3 = 0;
}

/* ---------- 2D ---------- */

void batch2d_begin(int target_w, int target_h) {
    g_n2 = 0;
    g_w2 = target_w;
    g_h2 = target_h;
}

static void v2push(float x, float y, float r, float g, float b, float a) {
    if (g_n2 >= MAX_V2) return;
    float *q = g_v2 + (size_t)g_n2 * 6;
    q[0] = x / g_w2 * 2.0f - 1.0f;
    q[1] = 1.0f - y / g_h2 * 2.0f;
    q[2] = r; q[3] = g; q[4] = b; q[5] = a;
    g_n2++;
}

void batch2d_rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    v2push(x, y, r, g, b, a); v2push(x + w, y, r, g, b, a); v2push(x + w, y + h, r, g, b, a);
    v2push(x, y, r, g, b, a); v2push(x + w, y + h, r, g, b, a); v2push(x, y + h, r, g, b, a);
}

void batch2d_flush(void) {
    if (!g_n2) return;
    glUseProgram(g_prog2);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(g_vao2);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo2);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(float) * 6 * (size_t)g_n2), g_v2, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, g_n2);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    g_n2 = 0;
}
