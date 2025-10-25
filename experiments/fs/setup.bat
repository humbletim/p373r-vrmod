@echo off
setlocal enabledelayedexpansion

REM set snapshot_dir=fs-7.1.13-devtime-avx2
if not "%base:fs-=%" == "%base%" (set devtime=fs-devtime)
if not "%base:sl-=%" == "%base%" (set devtime=sl-devtime)

if not exist p373r-vrmod-devtime ( echo missing p373r-vrmod-devtime && exit /b 1)
if not exist %snapshot_dir% ( echo missing %snapshot_dir% && exit /b 1)
if not defined devtime ( echo missing devtime && exit /b 1)

if DEFINED GITHUB_ACTIONS (
    @call p373r-vrmod-devtime\experiments\winsdk.in\other\vs-emulate-xwin.bat
    if errorlevel 1 (echo error gha vs-emulate-xwin && exit /b 1)
)

if DEFINED GITHUB_ACTIONS (
    for %%I in (clang++.exe) do ( set "CLANG_EXE_PATH=%%~$PATH:I" )
    if defined CLANG_EXE_PATH (
        echo clang++.exe found at: !CLANG_EXE_PATH!
        for %%J in ("!CLANG_EXE_PATH!\..") do (
            for %%K in ("%%~fJ\..") do ( set "LLVM_DIR=%%~fK" )
        )
        echo LLVM directory is: !LLVM_DIR!
        mklink /D llvm "!LLVM_DIR!"
        if errorlevel 1 ( echo ERROR: Failed to create symbolic link && exit /b 1)
    )
)

@call p373r-vrmod-devtime\experiments\portables\setup-portables.bat
if errorlevel 1 (echo error provisioning setup-portables && exit /b 1)

set PATH=bin;llvm\bin;pg\bin;pg\usr\bin;pg\mingw64\bin
set LIB=NUL

@echo off
set "_llvm="
for /d %%F in ("llvm\lib\clang\*") do (
    if exist "%%F\include" (
        set "_llvm=%%F\include"
        set "_llvm=!_llvm:\=/!"
    )
)
echo _llvm=%_llvm%
if NOT DEFINED _llvm (echo error provisioning _llvm && exit /b 1)

REM set _llvm=llvm/lib/clang/19/include
set winsdk=winsdk
envsubst.exe < p373r-vrmod-devtime/experiments/winsdk.in/winsdk.rsp > winsdk/winsdk.rsp
envsubst.exe < p373r-vrmod-devtime/experiments/winsdk.in/mm.rsp > winsdk/mm.rsp
jq.exe -s ".[0] * { roots: (.[0].roots + .[1].roots) }" winsdk/vfsoverlay.json p373r-vrmod-devtime/experiments/winsdk.in/vfsoverlay.extra.json > winsdk/_vfsoverlay.json 

if not exist %devtime% ( mkdir %devtime% )

envsubst.exe < %snapshot_dir%/llobjs.rsp.in > %devtime%/llobjs.rsp
envsubst.exe < %snapshot_dir%/llincludes.rsp.in > %devtime%/llincludes.rsp

envsubst.exe < %~dp0/devtime.in/compile.common.rsp > %devtime%/compile.common.rsp
envsubst.exe < %~dp0/devtime.in/link.common.rsp > %devtime%/link.common.rsp
if not "%base:fs-=%" == "%base%" (
    envsubst.exe < %~dp0/devtime.in/compile.fs.rsp > %devtime%/compile.rsp
    envsubst.exe < %~dp0/devtime.in/link.fs.rsp > %devtime%/link.rsp
)
if not "%base:sl-=%" == "%base%" (
    envsubst.exe < %~dp0/devtime.in/compile.sl.rsp > %devtime%/compile.rsp
    envsubst.exe < %~dp0/devtime.in/link.sl.rsp > %devtime%/link.rsp
)

@REM envsubst.exe < %~dp0/devtime.in/custom-objs.rsp > %devtime%/custom-objs.rsp
envsubst.exe < %~dp0/devtime.in/application-bin.rsp > %devtime%/application-bin.rsp

