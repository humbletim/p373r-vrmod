@echo off

REM attempts to download and unpack an *intree* open source C++ "edge compiler" toolchain  
REM NOTE: this is a userspace strategy (ie: requires no admin rights or system level changes)
REM -- humbletim 2025.08.04

set portables=%~dp0

call %portables%\portable-jq.bat
if errorlevel 1 (echo error provisioning portable-jq && exit /b 1)

call %portables%\portable-winsdk.bat
if errorlevel 1 (echo error provisioning portable-winsdk && exit /b 1)

call %portables%\portable-ninja.bat
if errorlevel 1 (echo error provisioning portable-ninja && exit /b 1)

call %portables%\portable-git.bat
if errorlevel 1 (echo error provisioning portable-git && exit /b 1)

call %portables%\portable-clang++.bat
if errorlevel 1 (echo error provisioning portable-clang++ && exit /b 1)

REM call %portables%\portable-opengl32-mesa.bat
REM if errorlevel 1 (echo error provisioning portable-mesa && exit /b 1)

if errorlevel 1 (echo error provisioning portables && exit /b 1)
