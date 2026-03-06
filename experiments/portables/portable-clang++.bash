#!/bin/bash

set -euo pipefail

if apt-cache show clang-19 > /dev/null 2>&1 ; then
    echo "--- installing LLVM/Clang 19 via apt ---"
    export DEBIAN_FRONTEND=noninteractive
    sudo apt-get install -y -qq -o=Dpkg::Use-Pty=0 clang-19 lld-19 
else
    echo "--- installing LLVM/Clang 19 via llvm.sh ---"
    wget -O /tmp/llvm.sh https://apt.llvm.org/llvm.sh
    chmod +x /tmp/llvm.sh
    sudo /tmp/llvm.sh 19
fi

which clang++-19
