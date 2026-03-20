#!/bin/bash
set -euo pipefail

# Main driver to compile modded fs-open.exe

echo "==== Bootstrapping environment ===="
./bootstrap/fetch-fs-open.sh fs-open
./bootstrap/fetch-winsdk.sh winsdk
./bootstrap/setup-llvm.sh llvm

echo "==== Setting up symlinks ===="
cd community
ln -vsTf ../llvm llvm
ln -vsTf ../winsdk winsdk
touch winsdk/vfsoverlay.json

echo "==== Initializing TPVM environment ===="
../experiments/tpvm.sh init ../fs-open

echo "==== Applying patches and fetching local code ===="
# Use check logic before patching
if [ ! -f llviewerdisplay.cpp ] || ! grep -q "vrmod_llviewerdisplay_begin_render" llviewerdisplay.cpp; then
    rm -f llviewerdisplay.cpp
    ../experiments/tpvm.sh patch llviewerdisplay.cpp
else
    echo "llviewerdisplay.cpp already patched."
fi

if [ ! -f lldir_win32.cpp ]; then
    ../experiments/tpvm.sh mod lldir_win32.cpp
else
    echo "lldir_win32.cpp already modified."
fi

echo "==== Compiling Edge fs-open.exe ===="
make fs-open.exe -j

echo "==== Build Complete ===="
ls -l fs-open.exe
