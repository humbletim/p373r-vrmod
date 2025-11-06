@echo off

if not exist build ( mkdir build )

set PATH=bin;llvm\bin;pg\bin;pg\usr\bin;pg\mingw64\bin
set LIB=NUL
set vrmod=winxtropia-vrmod
REM set base=firestorm-7.1.13
REM set snapshot_dir=fs-7.1.13-devtime-avx2
REM set devtime=fs-devtime
if not "%base:fs-=%" == "%base%" (set devtime=fs-devtime)
if not "%base:sl-=%" == "%base%" (set devtime=sl-devtime)

set vrmod_dir=p373r-vrmod-devtime/sgeo-minimal

if not exist p373r-vrmod-devtime ( echo missing p373r-vrmod-devtime && exit /b 1)
if not exist %snapshot_dir% ( echo missing snapshot_dir=%snapshot_dir% && exit /b 1)
if not defined base ( echo missing base= && exit /b 1)

echo -- confirming toolchain isolation 
echo int main(^){ return 1^; } | llvm\bin\clang++.exe @winsdk/winsdk.rsp -x c++ - -llibcmt -o - -v 2>&1 | grep -Eo "[^""]+Visual Studio[^""]+" && (
  echo "NOPE!!!! Visual Studio is disrupting our LLVM/Clang++ isolated toolchain... :("
  exit /b 44
)

echo -- patch
if not exist build/%vrmod%.llviewerdisplay.cpp (
  patch --merge --ignore-whitespace -p1 %snapshot_dir%/source/newview/llviewerdisplay.cpp -i %vrmod_dir%/20251021-sgeo_min_vr_7.1.9-baseline-diff.patch -o build/%vrmod%.llviewerdisplay.cpp
  if errorlevel 1 ( echo running patch.exe ec=%errorlevel% && exit /b 25 )
)

if not exist build/%vrmod%.llviewerdisplay.cpp ( echo -- error patching llviewerdisplay.cpp && exit /b 15 ) 

echo -- otherstuff
@REM ------------------------------------------------- 
if not "%base:fs-=%" == "%base%" (
  if not exist build/%vrmod%.fsversionvalues.h (
    patch --merge --ignore-whitespace -p1 %snapshot_dir%/source/fsversionvalues.h -i %~dp0/fsversionvalues.h.patch -o build/%vrmod%.fsversionvalues.h
    if errorlevel 1 ( echo error running patch.exe ec=%errorlevel% && exit /b 25 )
  )
)

if not exist build/%vrmod%.llversioninfo.cpp ( cp -av %snapshot_dir%/source/newview/llversioninfo.cpp build/%vrmod%.llversioninfo.cpp )

call;
if errorlevel 1 ( echo error ec=%errorlevel% && exit /b 25 )
@REM ------------------------------------------------- 

echo -- vfsoverlay

if not exist build/%vrmod%.vfsoverlay.yaml (
  envsubst < %~dp0%~n0.yaml.in > build/%vrmod%.vfsoverlay.yaml
)

@REM if not exist %devtime%/boost-json-default_resource-instance_.o (
@REM   llvm\bin\clang++ @%devtime%/compile.rsp -c %~dp0/../../boost-json-default_resource-instance_.c++ -o %devtime%/boost-json-default_resource-instance_.o
@REM   llvm\bin\clang++ @%devtime%/compile.rsp -c %~dp0/../../boost-filesystem-detail-path_traits-convert.c++ -o %devtime%/boost-filesystem-detail-path_traits-convert.o
@REM   if errorlevel 1 ( echo ec=%errorlevel% && exit /b 25 )
@REM )

if not exist build/%vrmod%.vfsoverlay.yaml ( echo -- error generating llvm vfsoverlay && exit /b 30 ) 
if not exist build/%vrmod%.llviewerdisplay.cpp ( echo -- missing / failed build/%vrmod%.llviewerdisplay.cpp && exit /b 32 ) 
@REM if not exist %devtime%/boost-json-default_resource-instance_.o ( echo -- missing %devtime%/boost-json-default_resource-instance_.o && exit /b 51 ) 

set "echo_on=echo on&for %%. in (.) do"

@REM ------------------------------------------------- 
echo -- compile custom

if not exist build/%vrmod%.llversioninfo.cpp.obj ( %echo_on% llvm\bin\clang++.exe -w @%devtime%/compile.rsp @winsdk/mm.rsp -vfsoverlay build/%vrmod%.vfsoverlay.yaml -I%vrmod_dir% -I%snapshot_dir%/source build/%vrmod%.llversioninfo.cpp -c -o build/%vrmod%.llversioninfo.cpp.obj || ( echo error compiling ec=%errorlevel% && exit /b 25 ) )

@REM ------------------------------------------------- 
@echo off

echo -- compile modified llviewerdisplay.cpp

if not exist build/%vrmod%.llviewerdisplay.cpp.obj (
  %echo_on% llvm\bin\clang++.exe -w @%devtime%/compile.rsp @winsdk/mm.rsp -vfsoverlay build/%vrmod%.vfsoverlay.yaml -I%vrmod_dir% build/%vrmod%.llviewerdisplay.cpp -c -o build/%vrmod%.llviewerdisplay.cpp.obj || ( echo error compiling ec=%errorlevel% && exit /b 25 )
)

if not exist build/%vrmod%.llviewerdisplay.cpp.obj ( echo -- missing / failed build/%vrmod%.llviewerdisplay.cpp.obj && exit /b 32 ) 
if not exist build/%vrmod%.vfsoverlay.yaml ( echo -- error generating llvm vfsoverlay && exit /b 30 ) 

echo -- link build/%vrmod%.%base%.exe

@echo on
llvm\bin\clang++.exe @%devtime%/application-bin.rsp -vfsoverlay build/%vrmod%.vfsoverlay.yaml -o build/%vrmod%.%base%.exe || ( echo error linking ec=%errorlevel% && exit /b 64 )
@echo off

if not exist build/%vrmod%.%base%.exe ( echo -- error compiling application .exe && exit /b 44 ) 

echo -- done

dir build/%vrmod%.%base%.exe

for %%i in ("%snapshot_dir%") do (set "snapshot_dir=%%~fi")

echo -- creating build/%vrmod%.publish.bash
(
  echo . /d/a/_actions/humbletim/firestorm-gha/tpv-gha-nunja/gha/gha.upload-artifact.bash
  echo gha-upload-artifact-fast %vrmod%.%base% build/%vrmod%.%base%.exe 1 9
) > build/%vrmod%.publish.bash

echo -- creating boot bat
(
    echo @echo off
    echo set "EXEROOT=%snapshot_dir%\runtime"
    echo set "PATH=%%EXEROOT%%;%%PATH%%"
    echo build\\%vrmod%.%base%.exe %%*
) > build/%vrmod%.test.bat

echo -- ok, try running build/%vrmod%.test.bat to launch build/%vrmod%.%base%.exe inplace
