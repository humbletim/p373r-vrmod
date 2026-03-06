#!/bin/bash
set -euo pipefail

# Usage: ./setup-llvm.sh [target_dir]
TARGET_DIR="${1:-llvm}"

if [ -e "$TARGET_DIR" ] && [ -d "$TARGET_DIR" ]; then
    echo "[$0] llvm already exists at $TARGET_DIR."
else
    echo "[$0] Setting up llvm at $TARGET_DIR..."

    if [ -d "/usr/lib/llvm-19" ]; then
        echo "[$0] Found LLVM 19 at /usr/lib/llvm-19."
        ln -vsTf /usr/lib/llvm-19 "$TARGET_DIR"
    elif [ -d "/opt/humbletim/llvm" ]; then
        echo "[$0] Found LLVM 19 at /opt/humbletim/llvm."
        ln -vsTf /opt/humbletim/llvm "$TARGET_DIR"
    else
        echo "[$0] Fetching standalone LLVM 19 since standard paths were not found..."

        LLVM_VER="19.1.7"
        LLVM_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VER/clang+llvm-$LLVM_VER-x86_64-linux-gnu-ubuntu-22.04.tar.xz"

        TMP_DIR=$(mktemp -d)
        TMP_XZ="$TMP_DIR/llvm.tar.xz"

        wget -nv "$LLVM_URL" -O "$TMP_XZ"

        mkdir -p "$TMP_DIR/extracted"
        tar -C "$TMP_DIR/extracted" --strip-components=1 -xf "$TMP_XZ"

        mv "$TMP_DIR/extracted" "$TARGET_DIR"

        rm -rf "$TMP_DIR"
    fi

    echo "[$0] llvm successfully set up at $TARGET_DIR."
fi
