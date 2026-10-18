#!/bin/sh
# Builds the browser version of DOOM mode from the same doom_core.c the ATmega runs,
# and prints it as base64 for the DOOM_WASM constant in app/index.html.
# Needs clang with the wasm32 target and wasm-ld (e.g. apt install clang lld).
set -e
cd "$(dirname "$0")/.."
clang --target=wasm32 -O2 -nostdlib -ffreestanding -fno-builtin -Itools/wasm -I. \
  -Wl,--no-entry -Wl,--allow-undefined \
  -Wl,--export=doom_reset -Wl,--export=doom_key -Wl,--export=doom_update -Wl,--export=doom_render \
  -Wl,--export=doom_kills -Wl,--export=doom_health -Wl,--export=doom_take_events \
  doom_core.c tools/wasm/shim.c -o tools/wasm/doom.wasm
base64 -w0 tools/wasm/doom.wasm
