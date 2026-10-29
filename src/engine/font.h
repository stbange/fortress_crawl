#ifndef FC_FONT_H
#define FC_FONT_H

/* 5x7 bitmap font drawn at native pixel size into the current (low-res) target. */
void font_init(void);
void font_begin(int target_w, int target_h);
void font_text(int x, int y, const char *s, float r, float g, float b);
void font_end(void);
int font_text_width(const char *s);
#define FONT_LINE 9

#endif
