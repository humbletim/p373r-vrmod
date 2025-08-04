@echo off
set PG_VER=2.47.1

REM attempts to download and unpack an *intree* Portable Git+Bash for Windows environment
REM NOTE: this is a userspace strategy (ie: requires no admin rights or system level changes)
REM -- humbletim 2025.08.04


echo [portable-git version=%PG_VER%]

if exist pg\bin\bash.exe (
    echo -- existing portable-get found (detected pg/bin/bash.exe^)
    goto :done
)

if not exist tmp\PortableGit-%PG_VER%-64-bit.7z.exe (
    echo -- Downoading PortableGit-%PG_VER%-64-bit...
    if not exist tmp (mkdir tmp)
        %SYSTEMROOT%\System32\curl.exe -s -L -o tmp\PortableGit-%PG_VER%-64-bit.7z.exe https://github.com/git-for-windows/git/releases/download/v%PG_VER%.windows.1/PortableGit-%PG_VER%-64-bit.7z.exe
    if errorlevel 1 (echo error downloading PortableGit && exit /b 1)
)

echo -- Unpacking PortableGit-%PG_VER%-64-bit to pg/
    tmp\PortableGit-%PG_VER%-64-bit.7z.exe -o.\pg\ -y
if errorlevel 1 (echo error unpacking PortableGit && exit /b 1)

:done
echo [/portable-git]
