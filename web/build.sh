#!/usr/bin/env bash
# Builds the browser version into build-web/dist (index.html, index.js, index.wasm).
# Needs an activated Emscripten SDK: source /path/to/emsdk/emsdk_env.sh
set -euo pipefail
cd "$(dirname "$0")/.."
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
rm -rf build-web/dist
mkdir -p build-web/dist
cp build-web/index.html build-web/index.js build-web/index.wasm web/_headers build-web/dist/
echo "Done: build-web/dist. Try it with: python3 -m http.server -d build-web/dist 8000"
