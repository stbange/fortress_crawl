#!/usr/bin/env python3
"""Makes the GitHub social preview card (1280x640): a hall from the game, the title in the
game's own 5x7 font (read from src/engine/font.c), and a line about it.

    python3 tools/social_card.py [screenshot.png] [out.png]

The default screenshot comes from social_media/ (made by tools/screenshots.sh, not in git).

Upload the result once under the repository's Settings > General > Social preview.
"""
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
SHOT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "social_media/screenshots_1080p/hall_colossus.png"
OUT = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "docs/social_preview.png"
W, H = 1280, 640

# the glyphs: 64 rows of seven 5-bit lines for ASCII 32..95
src = (ROOT / "src/engine/font.c").read_text()
table = src[src.index("GLYPHS[64][7]"):src.index("};", src.index("GLYPHS[64][7]"))]
nums = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", table)]
GLYPHS = [nums[i * 7:i * 7 + 7] for i in range(64)]


def text_width(s, scale):
    return len(s) * 6 * scale - scale


def draw_text(img, x, y, s, scale, colour, shadow=None):
    d = ImageDraw.Draw(img)
    for pass_, (ox, oy, col) in enumerate([(scale // 2 + 1, scale // 2 + 1, shadow), (0, 0, colour)]):
        if col is None:
            continue
        cx = x
        for ch in s.upper():
            code = ord(ch) - 32
            if 0 <= code < 64:
                for row, bits in enumerate(GLYPHS[code]):
                    for col_ in range(5):
                        if bits & (0x10 >> col_):
                            px, py = cx + col_ * scale + ox, y + row * scale + oy
                            d.rectangle([px, py, px + scale - 1, py + scale - 1], fill=col)
            cx += 6 * scale


# the hall, cut clear of the HUD lines (top 90 and bottom 250 px of a 1080p shot) at 2:1
shot = Image.open(SHOT).convert("RGB")
sw, sh = shot.size
# paint out the crosshair in the middle with the pixels just left of it
k = sw // 480  # (the game draws at 480x270 and scales up)
cx, cy = sw // 2, sh // 2
patch = shot.crop((cx - 22 * k, cy - 6 * k, cx - 10 * k, cy + 6 * k))
shot.paste(patch, (cx - 10 * k, cy - 6 * k))
shot.paste(patch, (cx + 2 * k, cy - 6 * k))
top, bottom = int(sh * 90 / 1080), int(sh * 830 / 1080)
cw = (bottom - top) * 2
left = (sw - cw) // 2
card = shot.crop((left, top, left + cw, bottom)).resize((W, H), Image.NEAREST)

# darken toward the left and the bottom, where the words go
shade = Image.new("L", (W, H))
sp = shade.load()
for yy in range(H):
    for xx in range(W):
        a = max(0.0, 1.0 - xx / (W * 0.75)) * 0.78 + max(0.0, (yy - H * 0.55) / (H * 0.45)) * 0.55
        sp[xx, yy] = int(255 * min(0.88, a))
card = Image.composite(Image.new("RGB", (W, H), (8, 6, 10)), card, shade)

GOLD, EMBER, PALE, SHADOW = (255, 214, 110), (240, 140, 70), (225, 215, 195), (20, 10, 5)
x = 64
draw_text(card, x, 150, "FORTRESS", 14, GOLD, SHADOW)
draw_text(card, x, 270, "CRAWL", 14, GOLD, SHADOW)
draw_text(card, x + 4, 400, "AN ENDLESS FORTRESS. AN OLD KING.", 4, PALE, SHADOW)
draw_text(card, x + 4, 446, "RETRO 3D DUNGEON CRAWLER IN C - PLAY IN THE BROWSER", 3, EMBER, SHADOW)
draw_text(card, x + 4, 560, "GITHUB.COM/STBANGE/FORTRESS_CRAWL", 3, (150, 140, 125), SHADOW)

OUT.parent.mkdir(parents=True, exist_ok=True)
card.save(OUT, optimize=True)
print(OUT, card.size, OUT.stat().st_size // 1024, "KB")
