#!/bin/bash

set -euo pipefail

if apt-cache show clang-19 > /dev/null 2>&1 ; then
    echo "--- installing LLVM/Clang 19 via apt ---"
    export DEBIAN_FRONTEND=noninteractive
    sudo apt-get install -y -qq -o=Dpkg::Use-Pty=0 clang-19 lld-19 
elif /bin/true ; then
    LLVM_VER="19.1.7"
    LLVM_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VER/clang+llvm-$LLVM_VER-x86_64-linux-gnu-ubuntu-22.04.tar.xz"
    TMP_DIR=$(mktemp -d)
    TMP_XZ="$TMP_DIR/llvm.tar.xz"
    wget -nv "$LLVM_URL" -O "$TMP_XZ"
    mkdir -p "$TMP_DIR/extracted"
    tar -C "$TMP_DIR/extracted" --strip-components=1 -xf "$TMP_XZ"
    mv "$TMP_DIR/extracted" "$TARGET_DIR"
    rm -rf "$TMP_DIR"
else
    echo "--- installing LLVM/Clang 19 via llvm.sh ---"
    wget -O /tmp/llvm.sh https://apt.llvm.org/llvm.sh
    chmod +x /tmp/llvm.sh
    sudo /tmp/llvm.sh 19
fi

which clang++-19
