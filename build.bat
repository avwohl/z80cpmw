@echo off
REM Debug build. First argument x64 or ARM64, else this machine's own architecture.
setlocal
set "PLAT=%~1"
if not defined PLAT (
    if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" (set "PLAT=ARM64") else (set "PLAT=x64")
)
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" (set "MSBHOST=arm64") else (set "MSBHOST=amd64")
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\%MSBHOST%\MSBuild.exe" "%~dp0z80cpmw.sln" /p:Configuration=Debug /p:Platform=%PLAT% /v:minimal
