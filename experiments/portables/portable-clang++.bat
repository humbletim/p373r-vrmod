@echo off
set CLANG_VER=19.1.0

REM attempts to download and unpack an *intree* LLVM/Clang++ x86_64 toolchain
REM NOTE: this is a userspace strategy (ie: requires no admin rights or system level changes)
REM -- humbletim 2025.08.04

echo [portable-clang++ version=%CLANG_VER%]

if exist llvm\bin\clang++.exe (
    echo -- existing portable-clang++ found (detected llvm\bin\clang++.exe^)
    llvm\bin\clang++.exe --version
    goto :done
)

if not exist pg\mingw64\bin\xz.exe (
    echo "FIXME: currently portable-clang++ depends on portable-git.bat (pg\mingw64\bin\xz.exe)"
    exit /b 4
    goto :done
)

if not exist tmp (mkdir tmp)
if not exist tmp\clang+llvm-%CLANG_VER%-x86_64-pc-windows-msvc.tar.xz (
    echo -- download clang+llvm-%CLANG_VER%-x86_64-pc-windows-msvc...
      %SYSTEMROOT%\System32\curl.exe -s -L -o tmp\clang+llvm-%CLANG_VER%-x86_64-pc-windows-msvc.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.0/clang+llvm-%CLANG_VER%-x86_64-pc-windows-msvc.tar.xz
    if errorlevel 1 (echo error downloading clang+llvm && exit /b 1)
)

echo -- unpacking clang+llvm-%CLANG_VER%-x86_64-pc-windows-msvc.tar.xz to llvm/
if not exist llvm (mkdir llvm)
  pg\mingw64\bin\xz -v -k -d -c tmp\clang+llvm-%CLANG_VER%-x86_64-pc-windows-msvc.tar.xz | %SYSTEMROOT%\System32\tar.exe -xf - -Cllvm --strip-components=1
if errorlevel 1 (echo error unpacking clang+llvm && exit /b 1)

REM llvm lldb requires python310.dll and python310.zip
REM call %portables%\portable-clang++-python3.bat

:done
echo [/portable-clang++]
