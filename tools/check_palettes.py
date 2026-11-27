#!/usr/bin/env python3
"""Checks the colour palettes in src/game/palette.h (openspec accessibility: not by colour alone).

Each alternative palette is seen through its colour-vision deficiency (Machado, Oliveira and
Fernandes 2009, severity 1.0, applied in linear RGB) and every pair of the eight elements, and
every pair of the five rarities, must stay at least MIN_DE apart in CIELAB (CIE76). The normal
palette is checked as seen without a deficiency. Exits 1 if a pair is too close.
Usage: tools/check_palettes.py [palette.h]"""
import re, sys, itertools, os

MIN_DE = 20.0
MACHADO = {
    "protanopia":   [[0.152286, 1.052583, -0.204868], [0.114503, 0.786281, 0.099216], [-0.003882, -0.048116, 1.051998]],
    "deuteranopia": [[0.367322, 0.860646, -0.227968], [0.280085, 0.672501, 0.047413], [-0.011820, 0.042940, 0.968881]],
    "tritanopia":   [[1.255528, -0.076749, -0.178779], [-0.078411, 0.930809, 0.147602], [0.004733, 0.691367, 0.303900]],
}

def lin(c): return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
def lab(rgb):
    r, g, b = (lin(min(1.0, max(0.0, c))) for c in rgb)
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883
    f = lambda t: t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116
    return (116 * f(y) - 16, 500 * (f(x) - f(y)), 200 * (f(y) - f(z)))
def simulate(rgb, m):
    def enc(c): c = min(1.0, max(0.0, c)); return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055
    l = [lin(min(1.0, max(0.0, c))) for c in rgb]
    return tuple(enc(sum(m[i][k] * l[k] for k in range(3))) for i in range(3))
def de(a, b): return sum((x - y) ** 2 for x, y in zip(lab(a), lab(b))) ** 0.5

def tables(path):
    src = open(path).read()
    out = {}
    for name, body in re.findall(r"static const vec3 (\w+)\[[^\]]*\]\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S):
        pals = re.findall(r"\{\s*((?:\{[^{}]*\}\s*,?\s*)+)\}", body)
        out[name] = [[tuple(float(v.strip().rstrip('f')) for v in c.split(',')) for c in re.findall(r"\{([^{}]*)\}", p)] for p in pals]
    return out

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    path = args[0] if args else os.path.join(os.path.dirname(__file__), "..", "src", "game", "palette.h")
    t = tables(path)
    bad = 0
    views = [("normal", None), ("protanopia", MACHADO["protanopia"]), ("deuteranopia", MACHADO["deuteranopia"]), ("tritanopia", MACHADO["tritanopia"])]
    for table, names in (("ELEMENT_PAL", ["fire", "frost", "lightning", "poison", "arcane", "holy", "shadow", "earth"]),
                         ("RARITY_PAL", ["common", "magic", "rare", "epic", "legendary"])):
        for k, (view, m) in enumerate(views):
            cols = t[table][k]
            seen = [simulate(c, m) if m else c for c in cols]
            worst = min(((de(seen[i], seen[j]), names[i], names[j]) for i, j in itertools.combinations(range(len(cols)), 2)))
            ok = worst[0] >= MIN_DE
            bad += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {table:11s} {view:12s} closest {worst[1]}/{worst[2]}: {worst[0]:.1f}")
            # the normal palette seen with each deficiency: how much the alternatives help
            if k == 0 and "-v" in sys.argv:
                for v2, m2 in views[1:]:
                    s2 = [simulate(c, m2) for c in cols]
                    w2 = min(((de(s2[i], s2[j]), names[i], names[j]) for i, j in itertools.combinations(range(len(cols)), 2)))
                    print(f"     (normal palette with {v2}: closest {w2[1]}/{w2[2]} {w2[0]:.1f})")
    sys.exit(1 if bad else 0)

main()
