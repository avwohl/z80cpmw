@echo off
setlocal

REM Find Visual Studio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
)

if not defined VSINSTALL (
    set "VSINSTALL=C:\Program Files\Microsoft Visual Studio\18\Community"
)

REM Set up environment
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

echo === Compiling test harness ===
cd /d "%~dp0"

cl /nologo /EHsc /W3 /O2 ^
    /I z80cpmw /I z80cpmw/Core ^
    /D _CRT_SECURE_NO_WARNINGS ^
    test_emu.cpp ^
    z80cpmw/Core/qkz80.cc ^
    z80cpmw/Core/qkz80_errors.cc ^
    z80cpmw/Core/qkz80_mem.cc ^
    z80cpmw/Core/qkz80_reg_set.cc ^
    /Fe:test_emu.exe ^
    /link /SUBSYSTEM:CONSOLE

if errorlevel 1 (
    echo Compilation failed!
    exit /b 1
)

echo.
echo === Running test ===
echo.
test_emu.exe

endlocal
