#!/bin/bash
set -euo pipefail

# Usage: ./jules/compile.sh jules/temp/indra_newview_llstartup.cpp

INPUT_FILE="$1"
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file not found: $INPUT_FILE"
    exit 1
fi

BASENAME=$(basename "$INPUT_FILE")
OBJ_FILE="jules/build/${BASENAME}.obj"
SNAPSHOT_DIR="_snapshot/fs-7.2.2-avx2"

echo "Compiling $INPUT_FILE to $OBJ_FILE..."

LLVM_BIN="llvm/bin/clang++"
if [ ! -x "$LLVM_BIN" ]; then
    LLVM_BIN="clang++"
fi

# Note: We explicitly add source/viewer_components/login because lllogin.h is needed
# by llstartup.cpp and might not be in the default RSP includes.

$LLVM_BIN -c \
    @fs-devtime/compile.rsp \
    @winsdk/mm.rsp \
    -Ibuild/ \
    -I"${SNAPSHOT_DIR}/source" \
    -I"${SNAPSHOT_DIR}/source/viewer_components/login" \
    -o "$OBJ_FILE" \
    "$INPUT_FILE"

echo "Compilation successful: $OBJ_FILE"
