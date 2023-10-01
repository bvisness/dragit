#!/bin/bash
set -euo pipefail

ORCA_DIR=orca
STDLIB_DIR=$ORCA_DIR/src/libc-shim

wasmFlags="--target=wasm32 \
  --no-standard-libraries \
  -mbulk-memory \
  -g -O2 \
  -D__ORCA__ \
  -Wl,--no-entry \
  -Wl,--export-dynamic \
  -isystem $STDLIB_DIR/include \
  -I $ORCA_DIR/src \
  -I $ORCA_DIR/src/ext"

mkdir -p build
clang -MJbuild/compile_commands_raw.json $wasmFlags -o build/module.wasm $ORCA_DIR/src/orca.c $STDLIB_DIR/src/*.c src/main.c
echo "[" > build/compile_commands.json
cat build/compile_commands_raw.json >> build/compile_commands.json
echo "]" >> build/compile_commands.json

orca bundle --orca-dir $ORCA_DIR --name Dragit --resource-dir data --out-dir build build/module.wasm
