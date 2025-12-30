#!/bin/bash
set -euo pipefail

# Usage: ./jules/init_override.sh indra/newview/llstartup.cpp

INPUT_PATH="$1"
# Strip 'indra/' prefix if present to match snapshot structure
REL_PATH="${INPUT_PATH#indra/}"
SNAPSHOT_ROOT="_snapshot/fs-7.2.2-avx2/source"
SOURCE_FILE="${SNAPSHOT_ROOT}/${REL_PATH}"

if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: Source file not found at $SOURCE_FILE"
    exit 1
fi

# Flatten path for temp filename: indra_newview_llstartup.cpp
FLAT_NAME=$(echo "$INPUT_PATH" | sed 's/\//_/g')
TEMP_FILE="jules/temp/${FLAT_NAME}"

echo "Copying $SOURCE_FILE to $TEMP_FILE..."
cp "$SOURCE_FILE" "$TEMP_FILE"

# Check for patches
PATCH_FILE="jules/patches/${FLAT_NAME%.cpp}.patch"
if [ -f "$PATCH_FILE" ]; then
    echo "Applying patch $PATCH_FILE..."
    patch "$TEMP_FILE" "$PATCH_FILE"
fi

echo "Ready to edit: $TEMP_FILE"
