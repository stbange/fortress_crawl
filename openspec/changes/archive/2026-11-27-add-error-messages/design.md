# Design

## Decisions
- **The log's last lines** are kept by an SDL log hook in a ring; the crash handler writes the ring
  and a few numbers with write() and nothing that allocates.
- **The marker** lives in the store like a save, so the browser has it too (an abort leaves it).
- **No game download** without WebGL 2: the page gives Emscripten an instantiateWasm that never
  resolves, so the wasm is not fetched.
