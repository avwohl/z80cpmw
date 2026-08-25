@echo off
REM Build and run the headless test suites.
REM
REM Nothing here needs wxWidgets or the vcpkg tree: TerminalView.cpp includes
REM only pch.h (Win32 + the standard library), so the terminal conformance suite
REM compiles against the SDK alone and runs without a window. That is the point
REM of it - it can be run on any machine with a compiler, including one that
REM cannot build the app itself.

setlocal

REM Find Visual Studio the same way run_test.bat does.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
)
if not defined VSINSTALL (
    set "VSINSTALL=C:\Program Files\Microsoft Visual Studio\18\Community"
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo Could not initialise the MSVC environment.
    exit /b 1
)

cd /d "%~dp0.."
if not exist "obj\tests\vt52" mkdir "obj\tests\vt52"

echo === Building the terminal conformance suite ===
cl /nologo /EHsc /W3 /O2 /std:c++17 ^
    /D _CRT_SECURE_NO_WARNINGS ^
    /I z80cpmw ^
    tests\test_vt52.cpp ^
    z80cpmw\TerminalView.cpp ^
    /Fo:obj\tests\vt52\ ^
    /Fe:obj\tests\vt52\test_vt52.exe ^
    /link /SUBSYSTEM:CONSOLE user32.lib gdi32.lib
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo.
obj\tests\vt52\test_vt52.exe
if errorlevel 1 (
    echo.
    echo TERMINAL CONFORMANCE SUITE FAILED
    exit /b 1
)

echo.
echo All suites passed.
endlocal
