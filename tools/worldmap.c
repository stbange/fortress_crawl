/* Writes debug images of the world layout as PPM files.
 *
 *   worldmap places <cy> <radius> out.ppm          one pixel block per chunk: stratum/flavor colour,
 *                                                  structures outlined, the start marked white
 *   worldmap section <cz> <radius> out.ppm         vertical slice (x/y) through chunk row cz
 *   worldmap blocks <wy> <wx0> <wz0> <size> out.ppm  top-down block map of one world layer
 *   worldmap structs <radius>                       lists the halls, caverns and chasms near the gate
 *   worldmap story <chapters> <radius>              prints the seed's myth, the first chapters of the
 *                                                  metaplot and the domains and traces around the gate
 */
#include "world/layout.h"
#include "world/story.h"

static void write_ppm(const char *path, const u8 *rgb, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 3, (size_t)w * h, f);
    fclose(f);
}

static Col place_colour(const ChunkInfo *ci) {
    if (ci->arch == ARCH_EXTERIOR) return (Col){ 40, 90, 40 };
    const FlavorDef *f = &FLAVORS[ci->flavor];
    Col c = f->wall;
    /* darker for deep strata, lighter for high ones */
    float k = 0.6f + 0.06f * (float)ci->stratum;
    c.r = (u8)clampf(c.r * k, 0, 255);
    c.g = (u8)clampf(c.g * k, 0, 255);
    c.b = (u8)clampf(c.b * k, 0, 255);
    if (ci->has_struct) { c.r = (u8)MINI(255, c.r + 60); c.g = (u8)MINI(255, c.g + 60); }
    if (ci->arch == ARCH_VOID) { c.r /= 3; c.g /= 3; c.b /= 3; }
    return c;
}

static void chunk_map(const World *w, bool section, int fixed, int radius, const char *path) {
    int cells = radius * 2 + 1, px = 6;
    int W = cells * px, H = cells * px;
    u8 *img = calloc((size_t)W * H, 3);
    for (int j = 0; j < cells; j++)
        for (int i = 0; i < cells; i++) {
            int cx = i - radius, c2 = j - radius;
            ChunkInfo ci;
            if (section) world_chunk_info(w, cx, -c2, fixed, &ci);
            else world_chunk_info(w, cx, fixed, c2, &ci);
            Col c = place_colour(&ci);
            for (int y = 0; y < px; y++)
                for (int x = 0; x < px; x++) {
                    u8 *p = &img[(((size_t)j * px + y) * W + (size_t)i * px + x) * 3];
                    bool edge = x == 0 || y == 0;
                    p[0] = edge ? c.r / 2 : c.r;
                    p[1] = edge ? c.g / 2 : c.g;
                    p[2] = edge ? c.b / 2 : c.b;
                    if (cx == 0 && c2 == 0) p[0] = p[1] = p[2] = 255;
                }
        }
    write_ppm(path, img, W, H);
    free(img);
}

static void block_map(const World *w, int wy, int wx0, int wz0, int size, const char *path) {
    u8 *img = calloc((size_t)size * size, 3);
    Block *chunk = malloc(CHUNK_VOL);
    for (int cz = floordiv(wz0, CHUNK); cz <= floordiv(wz0 + size - 1, CHUNK); cz++)
        for (int cx = floordiv(wx0, CHUNK); cx <= floordiv(wx0 + size - 1, CHUNK); cx++) {
            ChunkInfo ci;
            world_generate_chunk(w, cx, floordiv(wy, CHUNK), cz, chunk, &ci);
            for (int lz = 0; lz < CHUNK; lz++)
                for (int lx = 0; lx < CHUNK; lx++) {
                    int x = cx * CHUNK + lx - wx0, z = cz * CHUNK + lz - wz0;
                    if (x < 0 || z < 0 || x >= size || z >= size) continue;
                    Block b = chunk[cidx(lx, floormod(wy, CHUNK), lz)];
                    Block below = floormod(wy, CHUNK) > 0 ? chunk[cidx(lx, floormod(wy, CHUNK) - 1, lz)] : B_ROCK;
                    u8 *p = &img[((size_t)z * size + x) * 3];
                    if (block_solid(b)) { p[0] = 70; p[1] = 64; p[2] = 60; }
                    else if (block_solid(below)) { p[0] = 200; p[1] = 190; p[2] = 160; }
                    else { p[0] = 20; p[1] = 20; p[2] = 30; }
                    if (BLOCKS[b].light) { p[0] = 255; p[1] = 160; p[2] = 40; }
                    if (b == B_WATER) { p[0] = 40; p[1] = 80; p[2] = 160; }
                }
        }
    write_ppm(path, img, size, size);
    free(chunk);
    free(img);
}

static void structs_dump(const World *w, int radius) {
    for (int cy = -radius / 2; cy <= radius / 2; cy++)
        for (int cz = -radius; cz <= radius; cz++)
            for (int cx = -radius; cx <= radius; cx++) {
                Structure s;
                if (!world_structure_at(w, cx, cy, cz, &s) || cx != s.cx0 || cy != s.cy0 || cz != s.cz0) continue;
                ChunkInfo ci;
                world_chunk_info(w, cx, cy, cz, &ci);
                char name[96];
                world_place_name(&ci, name, sizeof name);
                printf("%-40s chunks %d %d %d size %dx%dx%d, floor y %d (world centre %d %d %d)\n", name, s.cx0, s.cy0, s.cz0, s.sx,
                       s.sy, s.sz, s.floor_y, s.cx0 * CHUNK + s.sx * 16, s.floor_y, s.cz0 * CHUNK + s.sz * 16);
            }
}

static void story_dump(const World *w, int chapters, int radius) {
    StoryWorld sw;
    story_world(w->seed, &sw);
    printf("seed %llu: %s, raised by %s over %s. It fell on %s.\n\n", (unsigned long long)w->seed, sw.fortress, sw.builders,
           sw.sealed, sw.fall);
    Personal p;
    story_personal(w->seed, 0, &p);
    char buf[STORY_TEXT];
    for (int c = 0; c < chapters; c++) {
        Named t;
        story_chapter_target(w, c, &t);
        ChunkInfo ci;
        world_chunk_info(w, t.cx, t.cy, t.cz, &ci);
        if (c % 3 == 0) {
            story_arc_title(w, c / 3, buf, sizeof buf);
            printf("ARC %s\n", buf);
        }
        char place[96];
        world_place_name(&ci, place, sizeof place);
        printf("  chapter %d: %s, %s (monster %d x%.1f) at chunk %d %d %d, level %d, %s\n", c + 1, t.name, t.title, t.mon,
               t.scale, t.cx, t.cy, t.cz, ci.level, place);
        for (int m = 0; m < MOTIVE_COUNT; m++) {
            story_chapter_hook(w, m, c, &p, buf, sizeof buf);
            printf("    %-7s hook: %s\n", MOTIVE_NAMES[m], buf);
        }
        story_chapter_done(w, MOTIVE_REVENGE, c, &p, buf, sizeof buf);
        printf("    done (revenge): %s\n", buf);
        story_revelation(w, c, buf, sizeof buf);
        printf("    revelation: %s\n", buf);
    }
    printf("\nDOMAINS AND TRACES within %d chunks of the gate (y 0)\n", radius);
    u64 seen[512];
    int nseen = 0;
    for (int cz = -radius; cz <= radius; cz++)
        for (int cx = -radius; cx <= radius; cx++) {
            Domain d;
            story_domain(w, cx, 0, cz, &d);
            bool known = false;
            for (int i = 0; i < nseen; i++) known |= seen[i] == d.id;
            if (!known && nseen < 512) {
                seen[nseen++] = d.id;
                char line[128];
                story_domain_line(&d, line, sizeof line);
                Named l;
                story_lord(w, &d, &l);
                printf("  domain %s (%s, level %d): %s. Lord: %s, %s, at %d %d %d\n", d.name, CULTURE_NAMES[d.culture], d.level,
                       line, l.name, l.title, l.cx, l.cy, l.cz);
            }
            Trace t;
            if (story_trace(w, cx, 0, cz, &t)) {
                story_trace_text(w, cx, 0, cz, buf, sizeof buf);
                static const char *K[TR_COUNT] = { "stele", "corpse", "bounty" };
                printf("    %d %d %s%s: %s\n", cx, cz, K[t.kind], t.mission ? " (mission)" : "", buf);
            }
        }
}

int main(int argc, char **argv) {
    World w;
    const char *s = getenv("FC_SEED");
    world_init(&w, s ? strtoull(s, NULL, 10) : 1337);
    if (argc == 5 && strcmp(argv[1], "places") == 0) chunk_map(&w, false, atoi(argv[2]), atoi(argv[3]), argv[4]);
    else if (argc == 5 && strcmp(argv[1], "section") == 0) chunk_map(&w, true, atoi(argv[2]), atoi(argv[3]), argv[4]);
    else if (argc == 7 && strcmp(argv[1], "blocks") == 0)
        block_map(&w, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), argv[6]);
    else if (argc == 4 && strcmp(argv[1], "story") == 0) story_dump(&w, atoi(argv[2]), atoi(argv[3]));
    else if (argc == 3 && strcmp(argv[1], "structs") == 0) structs_dump(&w, atoi(argv[2]));
    else {
        fprintf(stderr, "usage:\n  worldmap places <cy> <radius> out.ppm\n  worldmap section <cz> <radius> out.ppm\n"
                        "  worldmap blocks <wy> <wx0> <wz0> <size> out.ppm\n  worldmap story <chapters> <radius>\n  worldmap structs <radius>\n");
        return 1;
    }
    return 0;
}
