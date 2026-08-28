@echo off
REM Build and run the headless test suites.
REM
REM Nothing here needs wxWidgets or the vcpkg tree: TerminalView.cpp and
REM emu_io_windows.cpp both include only pch.h (Win32 + the standard library),
REM so these suites compile against the SDK alone and run without a window.
REM That is the point of them - they can be run on any machine with a compiler,
REM including one that cannot build the app itself.
REM
REM The host-file suite additionally needs emu_io.h, which lives in the sibling
REM ..\romwbw_emu checkout along with the rest of the core.

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

REM The help suite is second because it can be.  HelpAssets.cpp includes
REM windows.h and the standard library; HelpWindow.cpp adds pch.h, resource.h
REM and Version.h and reaches no project symbol outside those two files.  The
REM suite itself reads no disk image and no help asset - its copy of
REM help_index.json is a string literal, so ..\ioscpm need not be checked out
REM either - and it needs no window station.  Both blocks further down exit /b 1
REM when ..\romwbw_emu or ..\cpmemu is missing, so a suite appended at the end
REM is unreachable on a machine that has only this repository, and this one has
REM no reason to be there.
REM
REM comdlg32.lib is deliberately absent: pch.h already pragma-comments
REM comctl32/shlwapi/winmm and HelpWindow.h pragma-comments winhttp, so the only
REM libraries left to name are the two the window and its font need.
if not exist "obj\tests\help" mkdir "obj\tests\help"

echo.
echo === Building the help renderer and asset suite ===
cl /nologo /EHsc /W3 /O2 /std:c++17 ^
    /D _CRT_SECURE_NO_WARNINGS ^
    /I z80cpmw ^
    tests\test_help.cpp ^
    z80cpmw\HelpAssets.cpp ^
    z80cpmw\HelpWindow.cpp ^
    /Fo:obj\tests\help\ ^
    /Fe:obj\tests\help\test_help.exe ^
    /link /SUBSYSTEM:CONSOLE user32.lib gdi32.lib
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo.
obj\tests\help\test_help.exe
if errorlevel 1 (
    echo.
    echo HELP RENDERER AND ASSET SUITE FAILED
    exit /b 1
)

REM The rendering suite is the only one here that needs a window station: it
REM creates a real window, asks the DWM to render it with PrintWindow, and
REM samples the pixels.  That is what settles a question like "is ESC[31m red",
REM which the model-level suite above cannot ask - cellAt() returns the index
REM that was stored, not the colour that reached the glass.  On a machine with
REM no interactive desktop it prints SKIP and exits 0 rather than failing.
if not exist "obj\tests\render" mkdir "obj\tests\render"

echo.
echo === Building the rendering conformance suite ===
cl /nologo /EHsc /W3 /O2 /std:c++17 ^
    /D _CRT_SECURE_NO_WARNINGS ^
    /I z80cpmw ^
    tests\test_render.cpp ^
    z80cpmw\TerminalView.cpp ^
    /Fo:obj\tests\render\ ^
    /Fe:obj\tests\render\test_render.exe ^
    /link /SUBSYSTEM:CONSOLE user32.lib gdi32.lib
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo.
obj\tests\render\test_render.exe
if errorlevel 1 (
    echo.
    echo RENDERING CONFORMANCE SUITE FAILED
    exit /b 1
)
if not exist "..\romwbw_emu\src\emu_io.h" (
    echo.
    echo Missing ..\romwbw_emu - clone it beside this repository to run the
    echo host file transfer suite.
    exit /b 1
)
if not exist "obj\tests\hostfile" mkdir "obj\tests\hostfile"

echo.
echo === Building the host file transfer suite ===
cl /nologo /EHsc /W3 /O2 /std:c++17 ^
    /D _CRT_SECURE_NO_WARNINGS ^
    /I z80cpmw /I ..\romwbw_emu\src ^
    tests\test_hostfile.cpp ^
    z80cpmw\emu_io_windows.cpp ^
    /Fo:obj\tests\hostfile\ ^
    /Fe:obj\tests\hostfile\test_hostfile.exe ^
    /link /SUBSYSTEM:CONSOLE user32.lib shell32.lib ole32.lib comdlg32.lib
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo.
obj\tests\hostfile\test_hostfile.exe
if errorlevel 1 (
    echo.
    echo HOST FILE TRANSFER SUITE FAILED
    exit /b 1
)

REM The HBIOS suite is the only one here that needs the Z80 core as well, since
REM it drives HBIOSDispatch::handleEXT with real guest registers and memory.
if not exist "..\cpmemu\src\qkz80.cc" (
    echo.
    echo Missing ..\cpmemu - clone it beside this repository to run the HBIOS
    echo host file extension suite.
    exit /b 1
)
if not exist "obj\tests\hbios" mkdir "obj\tests\hbios"

echo.
echo === Building the HBIOS host file extension suite ===
REM Two passes: the sibling core is compiled at /W1 because its own warnings
REM (C4244 in qkz80.cc) are not this repository's to fix, and thirty lines of
REM them would bury a warning that is.
cl /nologo /c /EHsc /W1 /O2 /std:c++17 ^
    /D _CRT_SECURE_NO_WARNINGS ^
    /I z80cpmw /I ..\cpmemu\src /I ..\romwbw_emu\src ^
    ..\romwbw_emu\src\hbios_dispatch.cc ^
    ..\romwbw_emu\src\hbios_cpu.cc ^
    ..\romwbw_emu\src\emu_init.cc ^
    ..\cpmemu\src\qkz80.cc ^
    ..\cpmemu\src\qkz80_errors.cc ^
    ..\cpmemu\src\qkz80_mem.cc ^
    ..\cpmemu\src\qkz80_reg_set.cc ^
    /Fo:obj\tests\hbios\
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)
cl /nologo /EHsc /W3 /O2 /std:c++17 ^
    /D _CRT_SECURE_NO_WARNINGS ^
    /I z80cpmw /I ..\cpmemu\src /I ..\romwbw_emu\src ^
    tests\test_hbios_hostfile.cpp ^
    z80cpmw\emu_io_windows.cpp ^
    obj\tests\hbios\hbios_dispatch.obj ^
    obj\tests\hbios\hbios_cpu.obj ^
    obj\tests\hbios\emu_init.obj ^
    obj\tests\hbios\qkz80.obj ^
    obj\tests\hbios\qkz80_errors.obj ^
    obj\tests\hbios\qkz80_mem.obj ^
    obj\tests\hbios\qkz80_reg_set.obj ^
    /Fo:obj\tests\hbios\ ^
    /Fe:obj\tests\hbios\test_hbios_hostfile.exe ^
    /link /SUBSYSTEM:CONSOLE user32.lib shell32.lib ole32.lib comdlg32.lib winmm.lib
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo.
obj\tests\hbios\test_hbios_hostfile.exe
if errorlevel 1 (
    echo.
    echo HBIOS HOST FILE EXTENSION SUITE FAILED
    exit /b 1
)

REM The configuration diagnostics suite sits last because it needs BOTH sibling
REM checkouts on the include path, and so has to come after the guard above:
REM Config.cpp includes EmulatorEngine.h, which includes hbios_cpu.h, which
REM includes qkz80.h.  Nothing from either sibling is linked - the suite stubs
REM EmulatorEngine::getUserDataDirectory() to point at %TEMP% and never builds
REM an engine - but the headers still have to be findable.
if not exist "obj\tests\config" mkdir "obj\tests\config"

echo.
echo === Building the configuration diagnostics suite ===
cl /nologo /EHsc /W3 /O2 /std:c++17 ^
    /D _CRT_SECURE_NO_WARNINGS ^
    /I z80cpmw /I ..\cpmemu\src /I ..\romwbw_emu\src ^
    tests\test_config.cpp ^
    z80cpmw\Config.cpp ^
    /Fo:obj\tests\config\ ^
    /Fe:obj\tests\config\test_config.exe ^
    /link /SUBSYSTEM:CONSOLE user32.lib shell32.lib ole32.lib
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo.
obj\tests\config\test_config.exe
if errorlevel 1 (
    echo.
    echo CONFIGURATION DIAGNOSTICS SUITE FAILED
    exit /b 1
)

echo.
echo All suites passed.
endlocal
