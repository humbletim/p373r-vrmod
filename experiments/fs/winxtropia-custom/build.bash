#!/bin/bash
set -euoa pipefail

mkdir -p build

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

envsubst < "${experiments}/fs/devtime.in/base.env.in" > "build/${vrmod}.${base}.env"

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
  cp -uav $snapshot_dir/source/newview/llviewerdisplay.cpp "build/$vrmod.llviewerdisplay.cpp" \
  || {
    if [ ! -f "build/$vrmod.llviewerdisplay.cpp" ]; then
        patch --merge --ignore-whitespace -p1 "$snapshot_dir/source/newview/llviewerdisplay.cpp" -i "$vrmod_dir/20251021-sgeo_min_vr_7.1.9-baseline-diff.patch" -o "build/$vrmod.llviewerdisplay.cpp"
    fi

    test -f "build/$vrmod.llviewerdisplay.cpp" || fail "-- error patching llviewerdisplay.cpp"
}

echo "-- otherstuff"
if [[ "$base" == *fs-* ]]; then
    if [ ! -f "build/$vrmod.fsversionvalues.h" ]; then
        sed 's/^#define LL_VIEWER_CHANNEL ".*"/#define LL_VIEWER_CHANNEL "Winxtropia-VR-GHA"/' "$snapshot_dir/source/fsversionvalues.h" > "build/$vrmod.fsversionvalues.h"
        # patch --merge --ignore-whitespace -p1 "$snapshot_dir/source/fsversionvalues.h" -i "$vrmod_root/fsversionvalues.h.patch" -o "build/$vrmod.fsversionvalues.h"
    fi
fi

if [ ! -f "build/$vrmod.llversioninfo.cpp" ]; then
    cp -av "$snapshot_dir/source/newview/llversioninfo.cpp" "build/$vrmod.llversioninfo.cpp"
fi

echo "-- vfsoverlay"
if [ ! -f "build/$vrmod.vfsoverlay.yaml" ]; then
    envsubst < "$vrmod_root/build.yaml.in" > "build/$vrmod.vfsoverlay.yaml"
    python3=$(which python3 || which python)
    ${python3} ${experiments}/vfstool.py merge build/$vrmod.vfsoverlay.yaml winsdk/_vfsoverlay.json -o build/vfsoverlay.yml
fi

if [ ! -f "build/vfsoverlay.yaml" ]; then
    python3=$(which python3 || which python)
    ${python3} ${experiments}/vfstool.py merge build/$vrmod.vfsoverlay.yaml winsdk/_vfsoverlay.json -o build/vfsoverlay.yml
fi

test -f "build/vfsoverlay.yaml" || fail "-- error generating llvm vfsoverlay"

cp -av $vrmod_root/placeholder-icon.ico build/
bash fs-devtime/env $vrmod_root/cosmetics.bash 

test -f "build/$vrmod.llviewerdisplay.cpp" || fail "-- missing / failed build/$vrmod.llviewerdisplay.cpp"


#set -x

echo "-- compile custom"
if [ ! -f "build/$vrmod.llversioninfo.cpp.obj" ]; then
    llvm/bin/clang++ -w @"$devtime/compile.rsp" @winsdk/mm.rsp -vfsoverlay build/vfsoverlay.yml -Ibuild/ -I"$vrmod_dir" -I"$snapshot_dir/source" "build/$vrmod.llversioninfo.cpp" -c -o "build/$vrmod.llversioninfo.cpp.obj"
fi

echo "-- compile modified llviewerdisplay.cpp"
if [ ! -f "build/$vrmod.llviewerdisplay.cpp.obj" ]; then
    llvm/bin/clang++ -w @"$devtime/compile.rsp" @winsdk/mm.rsp -vfsoverlay build/vfsoverlay.yml -Ibuild/ -I"$vrmod_dir" "build/$vrmod.llviewerdisplay.cpp" -c -o "build/$vrmod.llviewerdisplay.cpp.obj"
fi

test -f "build/$vrmod.llviewerdisplay.cpp.obj" || fail "-- missing / failed build/$vrmod.llviewerdisplay.cpp.obj"

echo "-- link $vrmod.$base"
llvm/bin/clang++ @"$devtime/application-bin.rsp" -vfsoverlay build/vfsoverlay.yml -Wl,/vfsoverlay:build/vfsoverlay.yml -o "build/$vrmod.$base.exe"

test -f "build/$vrmod.$base.exe" || fail "-- error compiling application "

echo "-- done"

ls -l "build/$vrmod.$base.exe"

envsubst < "${experiments}/fs/devtime.in/base.sln.in" > "build/${vrmod}.${base}.debug.sln"
envsubst < "${experiments}/fs/devtime.in/base.bat.in" > "build/${vrmod}.${base}.bat"

echo "-- creating build/$vrmod.publish.bash"
(
  echo . ./p373r-vrmod-devtime/experiments/gha-upload-artifact-fast.bash
  echo gha-upload-artifact-fast $vrmod.$base build/$vrmod.$base.exe 1 9 true
) > build/$vrmod.publish.bash

echo -- creating boot bash
(
    echo export "EXEROOT=$(readlink -f $snapshot_dir)/runtime"
    echo export "WINEPATH=\$EXEROOT"
    echo wine build/$vrmod.$base.exe "\$@"
) > build/$vrmod.test.bash

echo -- ok, try running build/$vrmod.test.bash to launch build/$vrmod.$base.exe inplace using Wine emulation

