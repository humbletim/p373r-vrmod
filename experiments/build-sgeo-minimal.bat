@echo off
set vrmod=sgeo-minimal-vrmod
set vrmod_dir=p373r-vrmod/sgeo-minimal
set vrpatch=%vrmod_dir%/0001-sgeo_min_vr_7.1.9-baseline-diff.patch
set snapshot_dir=fs-7.1.13-devtime-avx2

set PATH=bin;llvm\bin;pg\bin;pg\usr\bin;%PATH%

if exist %vrmod%.llviewerdisplay.cpp.rej ( dir *.rej && exit /b 7 )

echo -- patch
if not exist %vrmod%.llviewerdisplay.cpp (
  patch --merge --ignore-whitespace -p1 %snapshot_dir%/source/newview/llviewerdisplay.cpp -i %vrpatch% -o %vrmod%.llviewerdisplay.cpp
)
if exist %vrmod%.llviewerdisplay.cpp.rej ( dir *.rej && exit /b 13 )

if not exist %vrmod%.llviewerdisplay.cpp ( echo -- error patching llviewerdisplay.cpp && exit /b 15 ) 

echo -- vfsoverlay

if not exist %vrmod%.vfsoverlay.yaml (
  setlocal enabledelayedexpansion
  echo "%~n0.yaml.in => %vrmod%.vfsoverlay.yaml"
  if not exist %~dp0%~n0.yaml.in ( echo "%~dp0%~n0.yaml.in not found..." && exit /b 24 )
  (for /f "delims=" %%a in ('type "%~dp0%~n0.yaml.in"') do (
      set "line=%%a"
      set "line=!line:${snapshot_dir}=%snapshot_dir%!"
      set "line=!line:${vrmod}=%vrmod%!"
      set "line=!line:${vrmod_dir}=%vrmod_dir%!"
      echo(!line!
  )) > %vrmod%.vfsoverlay.yaml
)

if not exist %vrmod%.vfsoverlay.yaml ( echo -- error generating llvm vfsoverlay && exit /b 30 ) 
if not exist %vrmod%.llviewerdisplay.cpp ( echo -- missing / failed %vrmod%.llviewerdisplay.cpp && exit /b 32 ) 

echo -- compile modified llviewerdisplay.cpp

llvm\bin\clang++.exe @devtime/compile.rsp @winsdk/mm.rsp -vfsoverlay %vrmod%.vfsoverlay.yaml -I%vrmod_dir% %vrmod%.llviewerdisplay.cpp -c -o %vrmod%.llviewerdisplay.cpp.obj

if not exist %vrmod%.llviewerdisplay.cpp.obj ( echo -- missing / failed %vrmod%.llviewerdisplay.cpp.obj && exit /b 32 ) 
if not exist %vrmod%.vfsoverlay.yaml ( echo -- error generating llvm vfsoverlay && exit /b 30 ) 

echo -- link %vrmod%.firestorm-7.1.13.exe

llvm\bin\clang++.exe @devtime/application-bin.rsp @devtime/llobjs.rsp -vfsoverlay %vrmod%.vfsoverlay.yaml -o %vrmod%.firestorm-7.1.13.exe

if not exist %vrmod%.firestorm-7.1.13.exe ( echo -- error compiling application .exe && exit /b 44 ) 

echo -- done

dir %vrmod%.firestorm-7.1.13.exe

