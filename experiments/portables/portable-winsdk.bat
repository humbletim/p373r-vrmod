@echo off
setlocal
set XWIN_VER=0.6.5
set SDKVER=10.0.26100
set CRTVER=14.44.17.14
set SDKROOT=winsdk

REM attempts to download and unpack an *intree* set of compatible Windows SDK libraries and headers  
REM NOTE: this is a userspace strategy (ie: requires no admin rights or system level changes)
REM -- humbletim 2025.08.04

echo [portable-winsdk sdk-version=%SDKVER%]

set MANIFEST_OPT=
if exist %~dp0\manifest_17.%SDKVER%.json (
  set "MANIFEST_OPT=--manifest %~dp0\manifest_17.%SDKVER%.json"
)

if defined MANIFEST_OPT (
  echo -- winsdk using MANIFEST_OPT=%MANIFEST_OPT% 
)

if exist %SDKROOT%\sdk\include\um\Windows.h (
  echo -- winsdk already provisioned (detected %SDKROOT%\sdk\include\um\Windows.h^)
  goto :done
)

if not exist bin\xwin.exe (
  echo -- Downloading xwin...
  if not exist bin (mkdir bin)
  %SYSTEMROOT%\System32\curl.exe -s -L https://github.com/Jake-Shadle/xwin/releases/download/%XWIN_VER%/xwin-%XWIN_VER%-x86_64-pc-windows-msvc.tar.gz | %SYSTEMROOT%\System32\tar.exe -xf - -Cbin --strip-components=1 xwin-%XWIN_VER%-x86_64-pc-windows-msvc/xwin.exe
) else (
  echo -- Existing xwin.exe found
  bin\xwin.exe --version
)

REM --include-debug-libs 

echo -- downloading SDK components to tmp/ ...
  bin\xwin -Loff --cache-dir=tmp %MANIFEST_OPT% --crt-version %CRTVER% --sdk-version %SDKVER% download
if errorlevel 1 (echo error invoking xwin winsdk download && exit /b 1)

echo -- unpacking SDK components to %SDKROOT% ...
  bin\xwin --accept-license -Loff --cache-dir=tmp %MANIFEST_OPT% --crt-version %CRTVER% --sdk-version %SDKVER% splat --vfsoverlay --output %SDKROOT%
if errorlevel 1 (echo error invoking xwin winsdk splat && exit /b 1)

:done
echo [/portable-winsdk]
