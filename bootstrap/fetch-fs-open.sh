#!/bin/bash
set -euo pipefail

# Usage: ./fetch-fs-open.sh [target_dir]
TARGET_DIR="${1:-fs-open}"
FS_OPEN_URL="https://github.com/humbletim/firestorm-gha/releases/download/v7.2.3-vr-alpha-0/fs-open-80036-afc53db-afc53db-devtime.zip"

if [ -d "$TARGET_DIR" ]; then
    echo "[$0] fs-open already exists at $TARGET_DIR."
    # To keep idempotency and prevent exit stopping the caller if sourced (though we execute it), we just return 0 here.
    # Return 0 will cause bash to finish the script if not sourced, or return from function if sourced.
    # Better yet, use a conditional block.
else
    echo "[$0] Fetching fs-open into $TARGET_DIR..."

    TMP_ZIP=$(mktemp)
    wget -nv "$FS_OPEN_URL" -O "$TMP_ZIP"

    unzip -q "$TMP_ZIP"
    if [ "$TARGET_DIR" != "fs-open" ]; then
        mv fs-open "$TARGET_DIR"
    fi

    rm -f "$TMP_ZIP"

    echo "[$0] fs-open successfully fetched to $TARGET_DIR."
fi
