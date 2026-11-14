#!/bin/bash
# Records footage with the game's deterministic capture mode (--capture): steady 30 fps frames
# whatever the machine's speed, encoded with ffmpeg. Needs xvfb-run and ffmpeg.
#
#   tools/capture_clip.sh NAME WxH SECONDS game-options...
#
#   # a bot's play, from game second 634 (the bot runs at full speed until then):
#   tools/capture_clip.sh boss 1920x1080 30 --seed 1337 --bot quester --bot-minutes 40 --capture-start 634
#   # a demo scene with a slow turn (the demo needs --shot, and --shot-time keeps it from stopping):
#   tools/capture_clip.sh hall 1920x1080 12 --seed 1337 --shot /tmp/x.bmp --shot-time 9999 \
#       --demo place --pos 80 38 240 --capture-start 1.2 --capture-orbit 10
#   # walking toward the gate: --capture-drift 0.7
#
# Output: $OUT/NAME.mp4 (default social_media/clips). Fight scenes flash: check the result
# (a 1 s window should hold at most 3 big brightness jumps) and soften it with ffmpeg's tmix.
NAME=$1; SIZE=$2; SECS=$3; shift 3
W=${SIZE%x*}; H=${SIZE#*x}
BIN=${BIN:-build/fortress_crawl}
OUT=${OUT:-social_media/clips}
FR=$(mktemp -d); mkdir -p "$OUT"
HOME=$(mktemp -d) XDG_DATA_HOME=$HOME SDL_AUDIO_DRIVER=dummy \
    xvfb-run -a "$BIN" --fresh --mute --window "$W" "$H" --capture "$FR" --capture-seconds "$SECS" --capture-fps 30 "$@" >/dev/null 2>&1
ffmpeg -y -loglevel error -framerate 30 -i "$FR/%05d.bmp" -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p -movflags +faststart "$OUT/$NAME.mp4" && echo "ok   $OUT/$NAME.mp4"
rm -rf "$FR"
