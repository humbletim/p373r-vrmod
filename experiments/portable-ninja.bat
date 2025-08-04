@echo off
set NINJA_VER=1.10.2

REM attempts to download and unpack an *intree* Ninja builld tool
REM NOTE: this is a userspace strategy (ie: requires no admin rights or system level changes)
REM -- humbletim 2025.08.04

echo [portable-ninja version=%NINJA_VER%]

if exist bin\ninja.exe (
    echo -- bin\ninja.exe already exists!
    bin\ninja.exe --version
    goto :done
)

if not exist tmp (mkdir tmp)
if not exist tmp\ninja-%NINJA_VER%-win.zip (
    echo -- Downloading ninja v%NINJA_VER%...
      %SYSTEMROOT%\System32\curl.exe -s -L -o tmp\ninja-%NINJA_VER%-win.zip https://github.com/ninja-build/ninja/releases/download/v%NINJA_VER%/ninja-win.zip
    if errorlevel 1 (echo error downloading ninja && exit /b 1)
)

echo -- unpacking tmp\ninja-%NINJA_VER%-win.zip to bin/
  if not exist bin (mkdir bin)
  %SYSTEMROOT%\System32\tar.exe -C bin -xvf tmp\ninja-%NINJA_VER%-win.zip
if errorlevel 1 (echo error unpacking ninja && exit /b 1)
)

:done
echo [/portable-ninja]
