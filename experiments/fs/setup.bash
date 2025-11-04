#!/bin/bash
set -euo pipefail

if [ -z "${base:-}" ]; then
    echo "base= unspecified"
    exit 1
fi

# Determine devtime from base
devtime=""
if [[ "${base}" == *fs-* ]]; then
    devtime=fs-devtime
fi
if [[ "${base}" == *sl-* ]]; then
    devtime=sl-devtime
fi

if [ -z "$devtime" ]; then
    echo "could not determine devtime from base=${base}"
    exit 1
fi

# Pre-flight checks
if [ ! -d "p373r-vrmod-devtime" ]; then
    if [ -d experiments/winsdk.in ] ; then
       ln -s . p373r-vrmod-devtime
    else
      echo "missing p373r-vrmod-devtime"
    fi
    exit 1
fi

if [ ! -d "$snapshot_dir" ]; then
    echo "missing $snapshot_dir"
    exit 1
fi

# Symlink LLVM
CLANG_EXE_PATH=$(which clang-19) || true
if [ -n "$CLANG_EXE_PATH" ]; then
    echo "clang-19 found at: $CLANG_EXE_PATH"
    LLVM_DIR=$(dirname "$(dirname "$(readlink -f "$CLANG_EXE_PATH")")")
    echo "LLVM directory is: $LLVM_DIR"
    rm -f llvm
    ln -s "$LLVM_DIR" llvm
else
    echo "clang-19 not found in PATH"
    exit 1
fi

export PATH="$(pwd)/bin:$(pwd)/llvm/bin:$PATH"
unset LIB

# Find clang includes
_llvm=$(find llvm/lib/clang -path '*/include' -type d | head -n 1)
if [ -z "$_llvm" ]; then
    echo "error provisioning _llvm"
    exit 1
fi
echo "_llvm=$_llvm"

# Set winsdk variable
winsdk="winsdk"
mkdir -p "$winsdk"

if [ ! -f "winsdk/vfsoverlay.json" ]; then
  echo '{ "version": 0, "roots": [] }' > "winsdk/vfsoverlay.json"
fi

# envsubst setup
export base
export snapshot_dir
export devtime
export winsdk
export _llvm
safeenvsubst='envsubst '\''$base $snapshot_dir $devtime $winsdk $_llvm'\'

# Generate rsp files
eval "$safeenvsubst" < p373r-vrmod-devtime/experiments/winsdk.in/winsdk.rsp > winsdk/winsdk.rsp
eval "$safeenvsubst" < p373r-vrmod-devtime/experiments/winsdk.in/mm.rsp > winsdk/mm.rsp
jq -s ".[0] * { roots: (.[0].roots + .[1].roots) }" winsdk/vfsoverlay.json p373r-vrmod-devtime/experiments/winsdk.in/vfsoverlay.extra.json > winsdk/_vfsoverlay.json 

# Create devtime and generate more rsp files
mkdir -p "$devtime"
eval "$safeenvsubst" < "$snapshot_dir/llobjs.rsp.in" > "$devtime/llobjs.rsp"
eval "$safeenvsubst" < "$snapshot_dir/llincludes.rsp.in" > "$devtime/llincludes.rsp"

eval "$safeenvsubst" < "p373r-vrmod-devtime/experiments/fs/devtime.in/compile.common.rsp" > "$devtime/compile.common.rsp"
eval "$safeenvsubst" < "p373r-vrmod-devtime/experiments/fs/devtime.in/link.common.rsp" > "$devtime/link.common.rsp"

if [[ "$base" == *fs-* ]]; then
    eval "$safeenvsubst" < "p373r-vrmod-devtime/experiments/fs/devtime.in/compile.fs.rsp" > "$devtime/compile.rsp"
    eval "$safeenvsubst" < "p373r-vrmod-devtime/experiments/fs/devtime.in/link.fs.rsp" > "$devtime/link.rsp"
fi
if [[ "$base" == *sl-* ]]; then
    eval "$safeenvsubst" < "p373r-vrmod-devtime/experiments/fs/devtime.in/compile.sl.rsp" > "$devtime/compile.rsp"
    eval "$safeenvsubst" < "p373r-vrmod-devtime/experiments/fs/devtime.in/link.sl.rsp" > "$devtime/link.rsp"
fi

eval "$safeenvsubst" < "p373r-vrmod-devtime/experiments/fs/devtime.in/application-bin.rsp" > "$devtime/application-bin.rsp"

echo "setup.bash script created successfully."
