@echo off
set MESA_VER=24.3.2

echo [portable-opengl32-mesa version=%MESA_VER%]
if exist bin\opengl32.dll (
    echo -- skipping (bin\opengl32.dll already exists^)
    goto :done
)

if not exist tmp (mkdir tmp)
if not exist tmp\mesa-llvmpipe-x64-%MESA_VER%.zip (
    echo -- Downoading mesa-llvmpipe-x64...
      %SYSTEMROOT%\System32\curl.exe -s -L -o tmp\mesa-llvmpipe-x64-%MESA_VER%.zip https://github.com/mmozeiko/build-mesa/releases/download/%MESA_VER%/mesa-llvmpipe-x64-%MESA_VER%.zip
    if errorlevel 1 (echo error downloading mesa-llvmpipe-x64 && exit /b 1)
)
echo -- unpacking mesa-llvmpipe
if not exist bin (mkdir bin)

%SYSTEMROOT%\System32\tar.exe -Cbin -xvf tmp\mesa-llvmpipe-x64-%MESA_VER%.zip
if errorlevel 1 (echo error unpacking mesa-opengl32 && exit /b 1)

:done
echo [/portable-opengl32-mesa]
