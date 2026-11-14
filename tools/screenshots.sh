#!/bin/bash
# Regenerates the README's screenshots (docs/screenshots/*.png) from the current build, world
# seed 1337, with the game's own demo modes (see "--demo" in the README). Needs xvfb-run and
# python3 with Pillow.
#
#   tools/screenshots.sh                  every screenshot
#   tools/screenshots.sh forge_tab food   only these
#   OUT=/tmp/shots tools/screenshots.sh   write somewhere else
#   BIN=build/fortress_crawl              the game
#   SIZE=1920x1080 OUT=...                every shot at this size (social media: 1920x1080, 1080x1350)
#
# A shot that needs a place uses --demo place (the open floor of the chunk at --pos) or an
# exact --pos / --look; positions come from `build/worldmap structs 12` for seed 1337.

BIN=${BIN:-build/fortress_crawl}
OUT=${OUT:-docs/screenshots}
SEED=${SEED:-1337}
TMP=$(mktemp -d)
mkdir -p "$OUT"
ONLY=" $* "

want() { [ "$ONLY" = "  " ] || [[ "$ONLY" == *" $1 "* ]]; }

# shot NAME [WIDTHxHEIGHT] [game options...]
shot() {
    local name=$1 size=960x540
    shift
    if [[ "$1" =~ ^[0-9]+x[0-9]+$ ]]; then size=$1; shift; fi
    [ -n "$SIZE" ] && size=$SIZE
    want "$name" || return 0
    while [ "$(jobs -r | wc -l)" -ge "${PAR:-4}" ]; do sleep 0.5; done   # a few at a time
    (
        # an empty HOME: the start screen must not show someone's saves
        HOME="$TMP/home" XDG_DATA_HOME="$TMP/home" timeout 300 xvfb-run -a "$BIN" --seed "$SEED" --fresh --mute --shot "$TMP/$name.bmp" --shot-time "${SHOT_TIME:-3}" \
            --window "${size%x*}" "${size#*x}" "$@" >/dev/null 2>&1
        if [ -f "$TMP/$name.bmp" ]; then
            python3 -c "
from PIL import Image
Image.open('$TMP/$name.bmp').convert('RGB').save('$OUT/$name.png', optimize=True)"
            echo "ok   $name"
        else
            echo "FAIL $name"
        fi
    ) &
}

# ---- the world: outside, the vestibule, halls, regions (--debug names the place) ----
shot exterior
shot vestibule        --pos 8 7.5 0.5 --look 0 -3 --fly
shot hall_colossus    --demo place --pos 80 38 240
shot mines            --debug --demo place --pos -60 -176 60
shot gothic           --debug --demo place --pos -182 6 5
shot glacial          --debug --demo place --pos 22 6 -433
shot drowned          --debug --demo place --pos -800 6 100
shot ember            --debug --demo place --pos 8 7 774
shot jade             --debug --demo place --pos 800 8 120
shot heaven           --debug --demo place --pos 15 1744 2
shot suncourt         --debug --demo place --pos 15 1936 2
shot stormcrown       --debug --demo place --pos 15 2320 2

# ---- fights, spells, menus ----
shot combat           --demo combat
shot spells_cast      --demo magic
shot buffs            --demo buffs
shot skills           --demo skills
shot map              --demo map
shot inventory        --demo inventory
shot spellbook        --demo spells
shot loot             --demo loot
shot lost_arts        --demo lostarts
shot bestiary         --demo bestiary
shot bestiary_bosses  --demo bestiary --grade 2
shot start            1280x720 --demo menu
shot coop2            1280x720 --demo coop --players 2
shot coop3            1280x720 --demo coop --players 3

# ---- stories ----
shot story_motive     --demo motive
shot story_story      --demo story
shot story_read       --demo read
shot story_named      --demo named
shot golden_thread    --demo thread

# ---- the Sanctum: ruin, door, built, hall, decorations, shop, forge ----
shot sanctum          --demo sanctum --cast 0
shot sanctum_sky      --demo realm --pos 64.5 9625 30 --look 167 55 --fly
SHOT_TIME=7 shot sanctum_door --demo cast --cast 37
shot sanctum_built    --demo sanctum --cast 3 --pos 64 9670 150 --look -90 -38 --fly --radius 8
shot sanctum_hall     --demo sanctum --cast 2 --pos 46 9610.5 48 --look 20 -15 --fly
shot sanctum_decor    --demo sanctum --cast 3 --pos 48 9611 92 --look -60 -12 --fly
shot forge            --demo sanctum --cast 3 --pos 113 9610.5 92 --look -100 -14 --fly
shot shop             --demo shop --pos 47.5 9608 80.5

# ---- food, alchemy, the forge ----
shot wildlife         --demo wildlife
shot food             --demo food
shot alchemy          --demo alchemy
shot forge_tab        --demo forge
shot forge_session    --demo forgesession

wait

# ---- the layered world, from above: the gate's layer and the one below (worldmap) ----
if want layers && [ -x build/worldmap ]; then
    build/worldmap blocks 6 -30 -60 120 "$TMP/l0.ppm" >/dev/null 2>&1
    build/worldmap blocks -26 -30 -60 120 "$TMP/l1.ppm" >/dev/null 2>&1
    python3 -c "
from PIL import Image, ImageDraw, ImageFont
try: font = ImageFont.load_default(size=15)
except TypeError: font = ImageFont.load_default()
a, b = (Image.open('$TMP/l%d.ppm' % i).convert('RGB').resize((472, 472), Image.NEAREST) for i in (0, 1))
m = Image.new('RGB', (960, 540), (14, 12, 12))
m.paste(a, (6, 62)); m.paste(b, (482, 62))
d = ImageDraw.Draw(m)
d.text((10, 10), 'THE GATE LAYER', fill=(236, 205, 120), font=font); d.text((486, 10), 'THE LAYER BELOW', fill=(236, 205, 120), font=font)
d.text((10, 34), 'rooms and corridors, one floor each (the exterior and the vestibule at the left of the gate layer)', fill=(170, 160, 140), font=font)
m.save('$OUT/layers.png', optimize=True)" && echo "ok   layers"
fi

# ---- kept from earlier (framed by hand, their features have not changed) ----
# chasm, vault, puzzle_beam, puzzle_statues, trap_spikes, trap_flames, spell_keep, combat_mines.
# The demos --demo levers|beam|statues|spikes|flames|cast --cast 16 make
# similar views, but dim ones, and not in every seed.
