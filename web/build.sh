#!/usr/bin/env bash
# Builds the browser version into build-web/dist: index.html, index.js, index.wasm, index.data
# (the sounds) and music/ (fetched after the first frame).
# Needs an activated Emscripten SDK: source /path/to/emsdk/emsdk_env.sh
# With ffmpeg on the PATH the sounds and music are re-encoded as IMA ADPCM WAV (a quarter of the
# size, which SDL reads as it is); without it they go in as they are. FC_WEB_RAW_SFX=1 keeps the
# sounds uncompressed (sharp hits lose most to ADPCM: 14-19 dB SNR against 27-46 dB for the music).
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-web
adpcm() { # src_dir dst_dir
    mkdir -p "$2"
    for f in "$1"/*.wav; do
        out="$2/$(basename "$f")"
        [ "$out" -nt "$f" ] && continue
        ffmpeg -loglevel error -y -i "$f" -c:a adpcm_ima_wav "$out"
    done
}
SFX="$PWD/assets/sfx"
if command -v ffmpeg >/dev/null; then
    adpcm assets/music build-web/music-adpcm
    if [ "${FC_WEB_RAW_SFX:-0}" != 1 ]; then
        adpcm assets/sfx build-web/sfx-adpcm
        SFX="$PWD/build-web/sfx-adpcm"
    fi
    MUSIC=build-web/music-adpcm
else
    echo "ffmpeg not found: packing the sounds and music uncompressed"
    MUSIC=assets/music
fi
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DFC_WEB_SFX_DIR="$SFX"
cmake --build build-web -j
rm -rf build-web/dist
mkdir -p build-web/dist/music
cp build-web/index.html build-web/index.js build-web/index.wasm build-web/index.data web/_headers build-web/dist/
cp "$MUSIC"/*.wav build-web/dist/music/
du -sh build-web/dist/index.data build-web/dist/music | sed 's/^/  /'
echo "Done: build-web/dist. Try it with: python3 -m http.server -d build-web/dist 8000"
