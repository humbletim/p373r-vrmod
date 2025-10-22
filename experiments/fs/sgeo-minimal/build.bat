@echo off

set PATH=bin;llvm\bin;pg\bin;pg\usr\bin;pg\mingw64\bin
set LIB=NUL
set vrmod=sgeo-minimal-vrmod
set base=firestorm-7.1.13
set snapshot_dir=fs-7.1.13-devtime-avx2
set devtime=fs-devtime
set vrmod_dir=p373r-vrmod-devtime/sgeo-minimal

if not exist p373r-vrmod-devtime ( echo missing p373r-vrmod-devtime && exit /b 1)
if not exist %snapshot_dir% ( echo missing %snapshot_dir% && exit /b 1)

echo -- confirming toolchain isolation 
echo int main(^){ return 1^; } | llvm\bin\clang++.exe @winsdk/winsdk.rsp -x c++ - -llibcmt -o - -v 2>&1 | grep -Eo "[^""]+Visual Studio[^""]+" && (
  echo "NOPE!!!! Visual Studio is disrupting our LLVM/Clang++ isolated toolchain... :("
  exit /b 44
)

echo -- patch
if not exist %vrmod%.llviewerdisplay.cpp (
  patch --merge --ignore-whitespace -p1 %snapshot_dir%/source/newview/llviewerdisplay.cpp -i %vrmod_dir%/0001-sgeo_min_vr_7.1.9-baseline-diff.patch -o %vrmod%.llviewerdisplay.cpp
  if errorlevel 1 ( echo running patch.exe ec=%errorlevel% && exit /b 25 )
)

if not exist %vrmod%.llviewerdisplay.cpp ( echo -- error patching llviewerdisplay.cpp && exit /b 15 ) 

echo -- vfsoverlay

if not exist %vrmod%.vfsoverlay.yaml (
  envsubst < %~dp0%~n0.yaml.in > %vrmod%.vfsoverlay.yaml
)

if not exist %devtime%/boost-json-default_resource-instance_.o (
  llvm\bin\clang++ @%devtime%/compile.rsp -c %~dp0/../../boost-json-default_resource-instance_.c++ -o %devtime%/boost-json-default_resource-instance_.o
  if errorlevel 1 ( echo ec=%errorlevel% && exit /b 25 )
)

if not exist %vrmod%.vfsoverlay.yaml ( echo -- error generating llvm vfsoverlay && exit /b 30 ) 
if not exist %vrmod%.llviewerdisplay.cpp ( echo -- missing / failed %vrmod%.llviewerdisplay.cpp && exit /b 32 ) 
if not exist %devtime%/boost-json-default_resource-instance_.o ( echo -- missing %devtime%/boost-json-default_resource-instance_.o && exit /b 51 ) 

echo -- compile modified llviewerdisplay.cpp

@echo on
llvm\bin\clang++.exe -w @%devtime%/compile.rsp @winsdk/mm.rsp -vfsoverlay %vrmod%.vfsoverlay.yaml -I%vrmod_dir% %vrmod%.llviewerdisplay.cpp -c -o %vrmod%.llviewerdisplay.cpp.obj || ( echo error compiling ec=%errorlevel% && exit /b 25 )
@echo off

if not exist %vrmod%.llviewerdisplay.cpp.obj ( echo -- missing / failed %vrmod%.llviewerdisplay.cpp.obj && exit /b 32 ) 
if not exist %vrmod%.vfsoverlay.yaml ( echo -- error generating llvm vfsoverlay && exit /b 30 ) 

echo -- link %vrmod%.%base%.exe

@echo on
llvm\bin\clang++.exe @%devtime%/application-bin.rsp -vfsoverlay %vrmod%.vfsoverlay.yaml -o %vrmod%.%base%.exe || ( echo error linking ec=%errorlevel% && exit /b 64 )
@echo off

if not exist %vrmod%.%base%.exe ( echo -- error compiling application .exe && exit /b 44 ) 

echo -- done

dir %vrmod%.%base%.exe

for %%i in ("%snapshot_dir%") do (set "snapshot_dir=%%~fi")

echo -- creating boot bat
(
    echo @echo off
    echo set "EXEROOT=%snapshot_dir%\runtime"
    echo set "PATH=%%EXEROOT%%;%%PATH%%"
    echo %vrmod%.%base%.exe %%*
) > %vrmod%.test.bat

echo -- ok, try running %vrmod%.test.bat to launch ./%vrmod%.%base%.exe inplace
