@echo off
REM Build and run test_emu.cpp, the Z80/HBIOS console harness.
REM
REM The core is not in this repository. z80cpmw.vcxproj compiles it out of the
REM sibling checkouts and so does this script: ..\cpmemu\src for qkz80* and
REM ..\romwbw_emu\src for romwbw_mem.h. Both must be cloned beside this one.
REM
REM For the terminal conformance suite, which needs neither the core nor
REM wxWidgets, see tests\run_tests.bat.

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

cd /d "%~dp0"

if not exist "..\cpmemu\src\qkz80.cc" (
    echo Missing ..\cpmemu - clone it beside this repository.
    exit /b 1
)
if not exist "..\romwbw_emu\src\romwbw_mem.h" (
    echo Missing ..\romwbw_emu - clone it beside this repository.
    exit /b 1
)

if not exist "obj\tests\emu" mkdir "obj\tests\emu"

echo === Compiling test harness ===

cl /nologo /EHsc /W3 /O2 ^
    /I z80cpmw /I ..\cpmemu\src /I ..\romwbw_emu\src ^
    /D _CRT_SECURE_NO_WARNINGS ^
    test_emu.cpp ^
    ..\cpmemu\src\qkz80.cc ^
    ..\cpmemu\src\qkz80_errors.cc ^
    ..\cpmemu\src\qkz80_mem.cc ^
    ..\cpmemu\src\qkz80_reg_set.cc ^
    /Fo:obj\tests\emu\ ^
    /Fe:obj\tests\emu\test_emu.exe ^
    /link /SUBSYSTEM:CONSOLE

if errorlevel 1 (
    echo Compilation failed!
    exit /b 1
)

echo.
echo === Running test ===
echo.
obj\tests\emu\test_emu.exe

endlocal
