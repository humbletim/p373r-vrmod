#!/bin/bash
set -euo pipefail

# Usage: ./fetch-winsdk.sh [target_dir]
TARGET_DIR="${1:-winsdk}"

if [ -d "$TARGET_DIR" ] && [ -f "$TARGET_DIR/vfsoverlay.json" ]; then
    echo "[$0] winsdk already exists at $TARGET_DIR."
else
    echo "[$0] Fetching winsdk into $TARGET_DIR..."

    TMP_DIR=$(mktemp -d)
    TMP_TGZ="$TMP_DIR/xwin.tar.gz"

    wget -nv "https://github.com/Jake-Shadle/xwin/releases/download/0.8.0/xwin-0.8.0-x86_64-unknown-linux-musl.tar.gz" -O "$TMP_TGZ"

    mkdir -p "$TMP_DIR/bin"
    tar -C "$TMP_DIR/bin" --wildcards --strip-components=1 -xvf "$TMP_TGZ" '*/xwin'
    chmod a+x "$TMP_DIR/bin/xwin"

    SDKVER=10.0.26100
    CRTVER=14.44.17.14
    VARIANTS=desktop

    mkdir -p "$TMP_DIR/cache"
    "$TMP_DIR/bin/xwin" --accept-license -Loff --cache-dir="$TMP_DIR/cache" --variant "$VARIANTS" --crt-version "$CRTVER" --sdk-version "$SDKVER" splat --output "$TARGET_DIR"

    # touch "$TARGET_DIR/vfsoverlay.json"

    rm -rf "$TMP_DIR"
    echo "[$0] winsdk successfully fetched to $TARGET_DIR."
fi
