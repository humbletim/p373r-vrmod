@echo off
setlocal

FOR /F "tokens=*" %%g IN ('bash -c "cygpath -ms /c/Program\ File*/Microsoft\ Visual\ Studio/*/*/Common7/Tools/VsDevCmd.bat|sort -h|head -1"') DO (
    SET "vsdevcmd=%%g"
)

if "%vsdevcmd%" == "" (
    echo ERROR: detect_vsenv.bat: could not locate VsDevCmd.bat >&2
    del /Q "%before_file%" 2>nul
    exit /b 1
)

call "%vsdevcmd%" -no_logo -arch=x64 -host_arch=amd64 > nul

mkdir winsdk\sdk\lib\um
mkdir winsdk\sdk\lib\ucrt
mkdir winsdk\crt\lib
mklink /D winsdk\sdk\include "%WindowsSdkDir%\Include\%WindowsSDKVersion%"
mklink /D winsdk\crt\include "%VCToolsInstallDir%\include"
mklink /D winsdk\sdk\lib\um\x86_64 "%WindowsSdkDir%\Lib\%WindowsSDKVersion%\um\x64"
mklink /D winsdk\sdk\lib\ucrt\x86_64 "%WindowsSdkDir%\Lib\%WindowsSDKVersion%\ucrt\x64"
mklink /D winsdk\crt\lib\x86_64 "%VCToolsInstallDir%\lib\x64"

if not exist winsdk\vfsoverlay.json (
  echo { "version": 0, "roots": [] } > winsdk\vfsoverlay.json
)

exit /b 0
