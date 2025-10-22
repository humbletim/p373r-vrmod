@echo off
setlocal

FOR /F "tokens=*" %%g IN ('bash -c "cygpath -ms /c/Program\ File*/Microsoft\ Visual\ Studio/*/*/Common7/Tools/VsDevCmd.bat|sort -r|head -1"') DO (
    SET "vsdevcmd=%%g"
)

if "%vsdevcmd%" == "" (
    echo ERROR: detect_vsenv.bat: could not locate VsDevCmd.bat >&2
    del /Q "%before_file%" 2>nul
    exit /b 1
)

call "%vsdevcmd%" -no_logo -arch=x64 -host_arch=amd64 > nul

mkdir winsdk
mkdir winsdk\sdk
mkdir winsdk\crt
mklink /D winsdk\sdk\include "%WindowsSdkDir%\Include\%WindowsSDKVersion%"
mklink /D winsdk\sdk\lib "%WindowsSdkDir%\Lib\%WindowsSDKVersion%"
mklink /D winsdk\crt\include "%VCToolsInstallDir%\include"
mklink /D winsdk\crt\lib "%VCToolsInstallDir%\lib"

if not exist winsdk\vfsoverlay.json (
  echo { "version": 0, "roots": [] } > winsdk\vfsoverlay.json
)