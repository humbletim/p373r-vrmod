@echo off

if not exist p373r-vrmod-devtime ( echo missing p373r-vrmod-devtime && exit /b 1)

call p373r-vrmod-devtime\experiments\portables\setup-portables.bat

set PATH=bin;llvm\bin;pg\bin;pg\usr\bin;pg\mingw64\bin
set LIB=NUL

set _llvm=llvm/lib/clang/19/include
set winsdk=winsdk
envsubst.exe < p373r-vrmod-devtime/experiments/winsdk.in/winsdk.rsp > winsdk/winsdk.rsp
envsubst.exe < p373r-vrmod-devtime/experiments/winsdk.in/mm.rsp > winsdk/mm.rsp

if not exist fs-7.1.13-devtime-avx2 ( echo missing fs-7.1.13-devtime-avx2 && exit /b 1)

set snapshot_dir=fs-7.1.13-devtime-avx2
set devtime=fs-devtime

if not exist %devtime% ( mkdir %devtime% )

envsubst.exe < %snapshot_dir%/llobjs.rsp.in > %devtime%/llobjs.rsp
envsubst.exe < %snapshot_dir%/llincludes.rsp.in > %devtime%/llincludes.rsp

envsubst.exe < %~dp0/devtime.in/compile.rsp > %devtime%/compile.rsp
envsubst.exe < %~dp0/devtime.in/link.rsp > %devtime%/link.rsp
envsubst.exe < %~dp0/devtime.in/custom-objs.rsp > %devtime%/custom-objs.rsp
envsubst.exe < %~dp0/devtime.in/application-bin.rsp > %devtime%/application-bin.rsp

