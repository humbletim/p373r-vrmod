#!/bin/bash
set -euo pipefail

fail() { echo "$@" >&2 ; exit 1; }
test -v base || fail "missing base=[fs-x.y.z|sl-x.y.z]"

test -d $base && snapshot_dir=${snapshot_dir:-$base}

test -v snapshot_dir || fail "missing snapshot_dir=[fs-x.y.z-devtime|sl-x.y.z-devtime|fs-x.y.z-snapshot|etc]"

which envsubst >/dev/null || fail "missing envsubst -- please install (eg: sudo apt-get install -y gettext)"

# Determine devtime from base
devtime=""
if [[ "${base}" == *fs-* ]]; then
    devtime=fs-devtime
fi
if [[ "${base}" == *sl-* ]]; then
    devtime=sl-devtime
fi

test -n "$devtime" || fail "could not determine devtime from base=${base}"

# Pre-flight checks
if [ ! -d "p373r-vrmod-devtime" ]; then
    if [ -d experiments/winsdk.in ] ; then
       ln -s . p373r-vrmod-devtime
    else
       fail "missing p373r-vrmod-devtime"
    fi
fi

test -d "$snapshot_dir" || fail "missing $snapshot_dir"

# Symlink LLVM
if [ ! -x llvm/bin/clang ] ; then
    CLANG_EXE_PATH=$(which clang++) || true
    if [ -n "$CLANG_EXE_PATH" ]; then
        echo "clang++ found at: $CLANG_EXE_PATH"
        LLVM_DIR=$(dirname "$(dirname "$(readlink -f "$CLANG_EXE_PATH")")")
        echo "LLVM directory is: $LLVM_DIR"
        rm -f llvm
        if [[ "${RUNNER_OS:-}" == Windows ]] ; then
            cmd //c mklink //j llvm "$(cygpath -wa "$LLVM_DIR")"
        else
            ln -s "$LLVM_DIR" llvm
        fi
    else
        fail "clang++ not found in PATH"
    fi
fi

export PATH="$(pwd)/bin:$(pwd)/llvm/bin:$PATH"
unset LIB

# Find clang includes
_llvm=$(find llvm/lib/clang -path '*/include' -type d | head -n 1)
test -n "$_llvm" || fail "error provisioning _llvm"
echo "_llvm=$_llvm"

# Set winsdk variable
winsdk="winsdk"
mkdir -p "$winsdk"

if [ ! -f "winsdk/vfsoverlay.json" ]; then
  echo '{ "version": 0, "roots": [] }' > "winsdk/vfsoverlay.json"
fi


experiments=p373r-vrmod-devtime/experiments

# envsubst setup
export base
export snapshot_dir
export devtime
export winsdk
export _llvm
safeenvsubst() { envsubst '$base $snapshot_dir $devtime $winsdk $_llvm' ; }

# Generate rsp files
safeenvsubst < ${experiments}/winsdk.in/winsdk.rsp > winsdk/winsdk.rsp
safeenvsubst < ${experiments}/winsdk.in/mm.rsp > winsdk/mm.rsp
jq -s ".[0] * { roots: (.[0].roots + .[1].roots) }" ${experiments}/winsdk.in/vfsoverlay.extra.json winsdk/vfsoverlay.json > winsdk/_vfsoverlay.json 

# Create devtime and generate more rsp files
mkdir -p "$devtime"
safeenvsubst < "$snapshot_dir/llobjs.rsp.in" > "$devtime/llobjs.rsp"
safeenvsubst < "$snapshot_dir/llincludes.rsp.in" > "$devtime/llincludes.rsp"

tpl=$experiments/fs/devtime.in

safeenvsubst < "${tpl}/compile.common.rsp" > "$devtime/compile.common.rsp"
safeenvsubst < "${tpl}/link.common.rsp" > "$devtime/link.common.rsp"

if [[ "$base" == *fs-* ]]; then
    safeenvsubst < "${tpl}/compile.fs.rsp" > "$devtime/compile.rsp"
    safeenvsubst < "${tpl}/link.fs.rsp" > "$devtime/link.rsp"
fi
if [[ "$base" == *sl-* ]]; then
    safeenvsubst < "${tpl}/compile.sl.rsp" > "$devtime/compile.rsp"
    safeenvsubst < "${tpl}/link.sl.rsp" > "$devtime/link.rsp"
fi

safeenvsubst < "${tpl}/application-bin.rsp" > "$devtime/application-bin.rsp"

#safeenvsubst < "${tpl}/base.sln.in" > "$devtime/debug.sln"
safeenvsubst < "${tpl}/base.env.in" > "$devtime/env"

echo "setup.bash script created successfully."
