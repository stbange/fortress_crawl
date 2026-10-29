/* Writes debug images of the world layout as PPM files.
 *
 *   worldmap places <cy> <radius> out.ppm          one pixel block per chunk: stratum/flavor colour,
 *                                                  structures outlined, the start marked white
 *   worldmap section <cz> <radius> out.ppm         vertical slice (x/y) through chunk row cz
 *   worldmap blocks <wy> <wx0> <wz0> <size> out.ppm  top-down block map of one world layer
 */
#include "world/layout.h"

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

int main(int argc, char **argv) {
    World w;
    const char *s = getenv("FC_SEED");
    world_init(&w, s ? strtoull(s, NULL, 10) : 1337);
    if (argc == 5 && strcmp(argv[1], "places") == 0) chunk_map(&w, false, atoi(argv[2]), atoi(argv[3]), argv[4]);
    else if (argc == 5 && strcmp(argv[1], "section") == 0) chunk_map(&w, true, atoi(argv[2]), atoi(argv[3]), argv[4]);
    else if (argc == 7 && strcmp(argv[1], "blocks") == 0)
        block_map(&w, atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), argv[6]);
    else {
        fprintf(stderr, "usage:\n  worldmap places <cy> <radius> out.ppm\n  worldmap section <cz> <radius> out.ppm\n"
                        "  worldmap blocks <wy> <wx0> <wz0> <size> out.ppm\n");
        return 1;
    }
    return 0;
}
