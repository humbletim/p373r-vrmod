@echo off
set JQ_VER=1.8.1

REM attempts to download and unpack an *intree* Portable jq.exe for Windows environment

echo [portable-jq version=%JQ_VER%]


if exist bin\jq.exe (
    echo -- existing portable-jq found (detected bin/jq.exe^)
    goto :done
)

if not exist tmp\jq-%JQ_VER%-windows-amd64.exe (
    echo -- Downoading jq-%JQ_VER%-windows-amd64.exe...
    if not exist tmp (mkdir tmp)
        %SYSTEMROOT%\System32\curl.exe -s -L -o tmp\jq-%JQ_VER%-windows-amd64.exe https://github.com/jqlang/jq/releases/download/jq-%JQ_VER%/jq-windows-amd64.exe
    if errorlevel 1 (echo error downloading jq && exit /b 1)
)

echo -- Copying tmp\jq-%JQ_VER%-windows-amd64.exe to bin/
    copy tmp\jq-%JQ_VER%-windows-amd64.exe bin\jq.exe
if errorlevel 1 (echo error copying jq && exit /b 1)

:done
echo [/portable-jq]
