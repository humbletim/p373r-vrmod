#!/bin/bash

# identify and symbolic link llvm-compatible winsdk
# -- humbletim 2025.04.20

function gha-winsdk-vsdevenv-capture() {
    echo <<< EOF > gha-vsdevenv-shell-capture.bat
@echo off
mkdir winsdk
mkdir winsdk\sdk
mkdir winsdk\crt
mklink /D winsdk\sdk\include "%WindowsSdkDir%\Include\%WindowsSDKVersion%"
mklink /D winsdk\sdk\lib "%WindowsSdkDir%\Lib\%WindowsSDKVersion%"
mklink /D winsdk\crt\include "%VCToolsInstallDir%\include"
mklink /D winsdk\crt\lib "%VCToolsInstallDir%\lib"
EOF
    ./gha-vsdevenv-shell-capture.bat
}

function gha-winsdk-llvm-capture() {
    local winsdk=$(cygpath -ma winsdk)
    cat <<EOF > winsdk/winsdk.rsp
    -isystem$winsdk/crt/include -isystem$winsdk/sdk/include/ucrt -isystem$winsdk/sdk/include/shared -isystem$winsdk/sdk/include/um
    -L$winsdk/crt/lib/x86_64 -L$winsdk/sdk/lib/um/x86_64 -L$winsdk/sdk/lib/ucrt/x86_64
    -Wno-unused-command-line-argument -Wl,-ignore:4099 -DNOMINMAX -DNDEBUG
    --target=x86_64-pc-windows-msvc -Wno-msvc-not-found -nostdinc -nodefaultlibs -fuse-ld=lld
EOF
    local _llvm=$(cygpath -ms "$(dirname "$(which clang++)")/../lib/clang/"/*/include)
    cat <<EOF > winsdk/mm.rsp
    -D_INCLUDED_AMM -D_INCLUDED_IMM -D_INCLUDED_EMM -D_INCLUDED_NMM -D_INCLUDED_PMM -D_INCLUDED_SMM -D_INCLUDED_TMM -D_INCLUDED_WMM -D_INCLUDED_MM2 -D_MMINTRIN_H_INCLUDED -D_MMINTRIN_H_INCLUDED -D_INCLUDED_INTRIN -D__INTRIN_H_
    -isystem${_llvm} -include${_llvm}/mmintrin.h -include${_llvm}/xmmintrin.h -include${_llvm}/emmintrin.h -include${_llvm}/adcintrin.h
EOF
}

function gha-winsdk-snapshot-capture() {
    local artifact="$1" snapshot_dir="$2" base=${base:-${3:-$(basename "$2")}}
    (
        echo artifact=${artifact}
        echo snapshot_dir=${snapshot_dir}
        echo base=${base}
        echo prefix=${base//[^[:alnum:]]/-}
    ) | tee -a $GITHUB_ENV _snapshot/GITHUB_ENV
}

function gha-winsdk-devtime-capture() {( set -Eou pipefail ;
    export snapshot_dir="$1"
    test -f ${snapshot_dir}/llobjs.rsp.in
    mkdir -pv devtime
    envsubst < ${snapshot_dir}/llobjs.rsp.in > devtime/llobjs.rsp
    envsubst < ${snapshot_dir}/llincludes.rsp.in > devtime/llincludes.rsp
    if [[ -f ${snapshot_dir}/lldefines.rsp.in ]] ; then
        envsubst < ${snapshot_dir}/lldefines.rsp.in > devtime/lldefines.rsp
    else
        cp -av ${snapshot_dir}/lldefines.rsp devtime/lldefines.rsp
    fi
    cat devtime/lldefines.rsp
    cat <<EOF > devtime/link.rsp
    @winsdk/winsdk.rsp
    -Wl,-ignore:4217 -Wl,-subsystem:windows -Wl,-machine:x64 -Wno-inconsistent-dllimport
    -lpsapi -ladvapi32 -lws2_32 -lmswsock -lrpcrt4 -lshell32 -lnetapi32 -lwer -ldbghelp -lgdi32 -lcomdlg32 -ldxgi -lopengl32 -limm32 -lwinmm -lcrypt32
    -L${snapshot_dir}/3p/lib -llibpng16 -lapr-1 -laprutil-1 -llibexpat -lminizip -lzlib -lmeshoptimizer -llibcollada14dom23-s -lfreetype -ljpeg -llibhunspell -lopenjp2 -llibcurl -llibssl -llibcrypto -lOpenAL32 -lalut -llibvorbisfile -llibvorbis -llibvorbisenc -llibogg -lnghttp2.lib -llibxml2 -Llxmlrpc-epi -llibndofdev -lhacd -lnd_hacdConvexDecomposition -lnd_pathing -lglu32 -lnvapi -ldiscord-rpc -lglod -lgrowl++
    ${snapshot_dir}/objs/llwebrtc.lib
    -llibboost_program_options-mt-x64 -llibboost_thread-mt-x64 -llibboost_url-mt-x64 -llibboost_fiber-mt-x64 -llibboost_context-mt-x64 -llibboost_wave-mt-x64 -llibboost_filesystem-mt-x64
EOF
    cat <<EOF > devtime/compile.rsp
    @winsdk/winsdk.rsp @devtime/llincludes.rsp @devtime/lldefines.rsp
    -DLL_WINDOWS
    -isystem${snapshot_dir}/3p/include
    -w -std=c++20 -isystem${snapshot_dir}/3p/include/apr-1
    -fms-runtime-lib=dll -pthread
EOF
    echo "working around boost::json::default_resource::instance_ msvc-vs-llvm quirk" >&2
    echo "boost::json::detail::default_resource boost::json::detail::default_resource::instance_;" | clang++ @devtime/compile.rsp -includeboost/json.hpp -x c++ -c - -o devtime/bjson.o
    echo devtime/bjson.o > devtime/custom-objs.rsp
    echo '@devtime/link.rsp @devtime/llobjs.rsp @devtime/custom-objs.rsp' > devtime/application-bin.rsp
)}
