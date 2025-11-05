#!/bin/bash
set -euoa pipefail

fail() { echo "$@" >&2 ; exit 1; }
test -v base || fail "missing base=[fs-x.y.z|sl-x.y.z]"
test -v snapshot_dir || fail "missing snapshot_dir=[fs-x.y.z-devtime|sl-x.y.z-devtime|fs-x.y.z-snapshot|etc]"

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

export PATH="$(pwd)/bin:$(pwd)/llvm/bin:$PATH"
unset LIB

vrmod=winxtropia-vrmod
experiments="p373r-vrmod-devtime/experiments"
vrmod_root=$experiments/fs/winxtropia-custom
vrmod_dir="p373r-vrmod-devtime/sgeo-minimal"

envsubst < "${experiments}/fs/devtime.in/base.env.in" > "${vrmod}.${base}.env"

test -d "p373r-vrmod-devtime" || fail "missing p373r-vrmod-devtime"
test -d "$snapshot_dir" || fail "missing snapshot_dir=$snapshot_dir"

# this is Windows (Visual Studio) specific... can be skipped on linux
# echo "-- confirming toolchain isolation"
# if echo "int main(){ return 1; }" | llvm/bin/clang++ @winsdk/winsdk.rsp -x c++ - -llibcmt -o - -v 2>&1 | grep -E "[^[:print:]]+Visual Studio[^[:print:]]+"; then
#     echo "NOPE!!!! Visual Studio is disrupting our LLVM/Clang++ isolated toolchain... :("
#     exit 44
# fi

echo "-- detect P373R-prepatched or apply VR patch"
grep P373R $snapshot_dir/source/newview/llviewerdisplay.cpp >/dev/null && \
  cp -uav $snapshot_dir/source/newview/llviewerdisplay.cpp "$vrmod.llviewerdisplay.cpp" \
  || {
    if [ ! -f "$vrmod.llviewerdisplay.cpp" ]; then
        patch --merge --ignore-whitespace -p1 "$snapshot_dir/source/newview/llviewerdisplay.cpp" -i "$vrmod_dir/20251021-sgeo_min_vr_7.1.9-baseline-diff.patch" -o "$vrmod.llviewerdisplay.cpp"
    fi

    test -f "$vrmod.llviewerdisplay.cpp" || fail "-- error patching llviewerdisplay.cpp"
}

echo "-- otherstuff"
if [[ "$base" == *fs-* ]]; then
    if [ ! -f "$vrmod.fsversionvalues.h" ]; then
        sed 's/^#define LL_VIEWER_CHANNEL ".*"/#define LL_VIEWER_CHANNEL "Winxtropia-VR-GHA"/' "$snapshot_dir/source/fsversionvalues.h" > "$vrmod.fsversionvalues.h"
        # patch --merge --ignore-whitespace -p1 "$snapshot_dir/source/fsversionvalues.h" -i "$vrmod_root/fsversionvalues.h.patch" -o "$vrmod.fsversionvalues.h"
    fi
fi

if [ ! -f "$vrmod.llversioninfo.cpp" ]; then
    cp -av "$snapshot_dir/source/newview/llversioninfo.cpp" "$vrmod.llversioninfo.cpp"
fi

echo "-- vfsoverlay"
if [ ! -f "$vrmod.vfsoverlay.yaml" ]; then
    envsubst < "$vrmod_root/build.yaml.in" > "$vrmod.vfsoverlay.yaml"
fi

test -f "$vrmod.vfsoverlay.yaml" || fail "-- error generating llvm vfsoverlay"
test -f "$vrmod.llviewerdisplay.cpp" || fail "-- missing / failed $vrmod.llviewerdisplay.cpp"

#set -x

echo "-- compile custom"
if [ ! -f "$vrmod.llversioninfo.cpp.obj" ]; then
    llvm/bin/clang++ -w @"$devtime/compile.rsp" @winsdk/mm.rsp -vfsoverlay "$vrmod.vfsoverlay.yaml" -I"$vrmod_dir" -I"$snapshot_dir/source" "$vrmod.llversioninfo.cpp" -c -o "$vrmod.llversioninfo.cpp.obj"
fi

echo "-- compile modified llviewerdisplay.cpp"
if [ ! -f "$vrmod.llviewerdisplay.cpp.obj" ]; then
    llvm/bin/clang++ -w @"$devtime/compile.rsp" @winsdk/mm.rsp -vfsoverlay "$vrmod.vfsoverlay.yaml" -I"$vrmod_dir" "$vrmod.llviewerdisplay.cpp" -c -o "$vrmod.llviewerdisplay.cpp.obj"
fi

test -f "$vrmod.llviewerdisplay.cpp.obj" || fail "-- missing / failed $vrmod.llviewerdisplay.cpp.obj"

echo "-- link $vrmod.$base"
llvm/bin/clang++ @"$devtime/application-bin.rsp" -vfsoverlay "$vrmod.vfsoverlay.yaml" -o "$vrmod.$base.exe"

test -f "$vrmod.$base.exe" || fail "-- error compiling application "

echo "-- done"

ls -l "$vrmod.$base.exe"

envsubst < "${experiments}/fs/devtime.in/base.sln.in" > "${vrmod}.${base}.debug.sln"

