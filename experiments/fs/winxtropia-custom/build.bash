#!/bin/bash
set -euo pipefail

export PATH="$(pwd)/bin:$(pwd)/llvm/bin:$PATH"
unset LIB

vrmod=winxtropia-vrmod

devtime=""
if [[ "$base" == *fs-* ]]; then
    devtime=fs-devtime
fi
if [[ "$base" == *sl-* ]]; then
    devtime=sl-devtime
fi
if [ -z "$devtime" ]; then
    echo "could not determine devtime from base=$base"
    exit 1
fi

vrmod_dir="p373r-vrmod-devtime/sgeo-minimal"

if [ ! -d "p373r-vrmod-devtime" ]; then
    echo "missing p373r-vrmod-devtime"
    exit 1
fi
if [ ! -d "$snapshot_dir" ]; then
    echo "missing snapshot_dir=$snapshot_dir"
    exit 1
fi
if [ -z "$base" ]; then
    echo "missing base="
    exit 1
fi

# this is Windows (Visual Studio) specific... can be skipped on linux
# echo "-- confirming toolchain isolation"
# if echo "int main(){ return 1; }" | llvm/bin/clang++ @winsdk/winsdk.rsp -x c++ - -llibcmt -o - -v 2>&1 | grep -E "[^[:print:]]+Visual Studio[^[:print:]]+"; then
#     echo "NOPE!!!! Visual Studio is disrupting our LLVM/Clang++ isolated toolchain... :("
#     exit 44
# fi

echo "-- detect P373R-prepatched or apply VR patch"
grep P373R $snapshot_dir/source/newview/llviewerdisplay.cpp && cp -uav $snapshot_dir/source/newview/llviewerdisplay.cpp "$vrmod.llviewerdisplay.cpp" || {
    if [ ! -f "$vrmod.llviewerdisplay.cpp" ]; then
        patch --merge --ignore-whitespace -p1 "$snapshot_dir/source/newview/llviewerdisplay.cpp" -i "$vrmod_dir/20251021-sgeo_min_vr_7.1.9-baseline-diff.patch" -o "$vrmod.llviewerdisplay.cpp"
    fi

    if [ ! -f "$vrmod.llviewerdisplay.cpp" ]; then
        echo "-- error patching llviewerdisplay.cpp"
        exit 15
    fi
}

echo "-- otherstuff"
if [[ "$base" == *fs-* ]]; then
    if [ ! -f "$vrmod.fsversionvalues.h" ]; then
        patch --merge --ignore-whitespace -p1 "$snapshot_dir/source/fsversionvalues.h" -i "p373r-vrmod-devtime/experiments/fs/winxtropia-custom/fsversionvalues.h.patch" -o "$vrmod.fsversionvalues.h"
    fi
fi

if [ ! -f "$vrmod.llversioninfo.cpp" ]; then
    cp -av "$snapshot_dir/source/newview/llversioninfo.cpp" "$vrmod.llversioninfo.cpp"
fi

echo "-- vfsoverlay"
if [ ! -f "$vrmod.vfsoverlay.yaml" ]; then
    envsubst < "p373r-vrmod-devtime/experiments/fs/winxtropia-custom/build.yaml.in" > "$vrmod.vfsoverlay.yaml"
fi

if [ ! -f "$vrmod.vfsoverlay.yaml" ]; then
    echo "-- error generating llvm vfsoverlay"
    exit 30
fi
if [ ! -f "$vrmod.llviewerdisplay.cpp" ]; then
    echo "-- missing / failed $vrmod.llviewerdisplay.cpp"
    exit 32
fi

set -x
echo "-- compile custom"
if [ ! -f "$vrmod.llversioninfo.cpp.obj" ]; then
    llvm/bin/clang++ -w @"$devtime/compile.rsp" @winsdk/mm.rsp -vfsoverlay "$vrmod.vfsoverlay.yaml" -I"$vrmod_dir" -I"$snapshot_dir/source" "$vrmod.llversioninfo.cpp" -c -o "$vrmod.llversioninfo.cpp.obj"
fi

echo "-- compile modified llviewerdisplay.cpp"
if [ ! -f "$vrmod.llviewerdisplay.cpp.obj" ]; then
    llvm/bin/clang++ -w @"$devtime/compile.rsp" @winsdk/mm.rsp -vfsoverlay "$vrmod.vfsoverlay.yaml" -I"$vrmod_dir" "$vrmod.llviewerdisplay.cpp" -c -o "$vrmod.llviewerdisplay.cpp.obj"
fi

if [ ! -f "$vrmod.llviewerdisplay.cpp.obj" ]; then
    echo "-- missing / failed $vrmod.llviewerdisplay.cpp.obj"
    exit 32
fi

echo "-- link $vrmod.$base"
llvm/bin/clang++ @"$devtime/application-bin.rsp" -vfsoverlay "$vrmod.vfsoverlay.yaml" -o "$vrmod.$base.exe"

if [ ! -f "$vrmod.$base.exe" ]; then
    echo "-- error compiling application "
    exit 44
fi

echo "-- done"
ls -l "$vrmod.$base.exe"
