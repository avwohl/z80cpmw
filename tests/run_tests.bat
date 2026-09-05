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
REM windows.h, resource.h and the standard library; HelpWindow.cpp adds pch.h
REM and Version.h and reaches no project symbol outside those two files.  The
REM suite needs no disk image and no window station.  Both blocks further down
REM exit /b 1 when ..\romwbw_emu or ..\cpmemu is missing, so a suite appended at
REM the end is unreachable on a machine that has only this repository, and this
REM one has no reason to be there.
REM
REM ..\ioscpm is the exception, and it is handled by SKIPPING rather than by
REM exiting: this block is too high up to exit /b 1 from - everything below it
REM would stop running - and the sibling is needed by exactly one section of the
REM suite.  When it is there, z80cpmw.rc is compiled and linked in so the
REM bundled help resources are present in the test binary, and /D
REM HELP_BUNDLED_ASSETS turns on the section that checks each blob against the
REM file it came from.  When it is not, both are left out and that section
REM prints SKIP; the other sections do not read a help asset at all.
REM
REM The .rc compiled here is the APPLICATION'S, not a copy written for the
REM suite, which is the point: "no orphan resource" and "byte-identical to the
REM file" are then claims about the resource script that really ships.
REM
REM It needs two /i paths where the vcxproj needs one.  "..", the directory
REM holding the sibling checkouts, is the vcxproj's ResourceCompile
REM AdditionalIncludeDirectories in both configurations, and is what makes the
REM "ioscpm\release_assets\..." data files resolve.  "z80cpmw" is extra: MSBuild
REM runs rc.exe with the project directory as the working directory, so
REM #include "resource.h" and "Version.h" resolve beside the .rc there, while
REM this script runs from the repository root.
REM
REM comdlg32.lib is deliberately absent: pch.h already pragma-comments
REM comctl32/shlwapi/winmm and HelpWindow.h pragma-comments winhttp, so the only
REM libraries left to name are the two the window and its font need.
if not exist "obj\tests\help" mkdir "obj\tests\help"

set "HELPRES="
set "HELPDEF="
if exist "..\ioscpm\release_assets\help_index.json" (
    rc /nologo /i z80cpmw /i .. /fo obj\tests\help\z80cpmw.res z80cpmw\z80cpmw.rc
    if errorlevel 1 (
        echo Compiling z80cpmw.rc for the help suite failed.
        exit /b 1
    )
    set "HELPRES=obj\tests\help\z80cpmw.res"
    set "HELPDEF=/D HELP_BUNDLED_ASSETS"
) else (
    echo.
    echo Missing ..\ioscpm - clone it beside this repository to check the
    echo bundled help assets.  Skipping that section; the rest of the help
    echo suite still runs.
)

echo.
echo === Building the help renderer and asset suite ===
cl /nologo /EHsc /W3 /O2 /std:c++17 ^
    /D _CRT_SECURE_NO_WARNINGS %HELPDEF% ^
    /I z80cpmw ^
    tests\test_help.cpp ^
    z80cpmw\HelpAssets.cpp ^
    z80cpmw\HelpWindow.cpp ^
    %HELPRES% ^
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
REM The disk provenance suite needs the least of any of them: DiskLedger.cpp
REM holds no Win32, no WinHTTP and no file system, which is exactly why the
REM decisions it makes are checkable at all.  DiskHash.cpp is linked beside it
REM and does need windows.h and bcrypt.h - but not WinHTTP and not DiskCatalog,
REM which is the whole reason those two functions were split out of that class:
REM a wrong hash marks every image in the library as differing from the catalog,
REM and that is not a failure anyone can see by reading.  bcrypt.lib is named by
REM a pragma inside DiskHash.cpp, so it is absent from the link line here.
REM
REM It sits above the two sibling guards below for the same reason the help
REM suite does - everything after an exit /b 1 is unreachable on a machine that
REM has only this repository, and this suite has no reason to be there.
if not exist "obj\tests\ledger" mkdir "obj\tests\ledger"

echo.
echo === Building the disk provenance suite ===
cl /nologo /EHsc /W3 /O2 /std:c++17 ^
    /D _CRT_SECURE_NO_WARNINGS ^
    /I z80cpmw ^
    tests\test_diskledger.cpp ^
    z80cpmw\DiskLedger.cpp ^
    z80cpmw\DiskHash.cpp ^
    z80cpmw\DiskMigrationV0.cpp ^
    /Fo:obj\tests\ledger\ ^
    /Fe:obj\tests\ledger\test_diskledger.exe ^
    /link /SUBSYSTEM:CONSOLE
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo.
obj\tests\ledger\test_diskledger.exe
if errorlevel 1 (
    echo.
    echo DISK PROVENANCE SUITE FAILED
    exit /b 1
)
REM The interface-v0 catalog suite needs no sibling either: CatalogV0.cpp holds
REM no Win32, no WinHTTP and no threads, which is the whole reason a suite can
REM drive it at all - DiskCatalog.cpp, which fetches these documents, cannot be
REM linked by anything.  So it sits above the sibling guards with the other two
REM that only need this repository.
if not exist "obj\tests\catalog" mkdir "obj\tests\catalog"

echo.
echo === Building the interface-v0 catalog suite ===
cl /nologo /EHsc /W3 /O2 /std:c++17 ^
    /D _CRT_SECURE_NO_WARNINGS ^
    /I z80cpmw ^
    tests\test_catalogv0.cpp ^
    z80cpmw\CatalogV0.cpp ^
    /Fo:obj\tests\catalog\ ^
    /Fe:obj\tests\catalog\test_catalogv0.exe ^
    /link /SUBSYSTEM:CONSOLE
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo.
obj\tests\catalog\test_catalogv0.exe
if errorlevel 1 (
    echo.
    echo INTERFACE-V0 CATALOG SUITE FAILED
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
REM
REM DiskMigrationV0.cpp is linked because Config.cpp's interface-v0 migration
REM calls it, and DiskLedger.cpp because DiskMigrationV0.cpp folds names with
REM DiskLedger::fold - the same fold the ledger keys are stored under, so that
REM "the same file" means one thing in both halves of the migration.
if not exist "obj\tests\config" mkdir "obj\tests\config"

echo.
echo === Building the configuration diagnostics suite ===
cl /nologo /EHsc /W3 /O2 /std:c++17 ^
    /D _CRT_SECURE_NO_WARNINGS ^
    /I z80cpmw /I ..\cpmemu\src /I ..\romwbw_emu\src ^
    tests\test_config.cpp ^
    z80cpmw\Config.cpp ^
    z80cpmw\DiskLedger.cpp ^
    z80cpmw\DiskMigrationV0.cpp ^
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
