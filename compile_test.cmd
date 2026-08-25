@echo off
REM Compile test_emu.cpp only, without running it. run_test.bat does both and
REM finds Visual Studio for itself; this is the one-liner form for a shell that
REM already has an MSVC environment.
REM
REM The core lives in the sibling checkouts, not in z80cpmw/Core - that
REM directory was removed when the core moved out, and the paths here followed.
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d C:\temp\src\z80cpmw
if not exist "obj\tests\emu" mkdir "obj\tests\emu"
cl /nologo /EHsc /W3 /O2 /I z80cpmw /I ..\cpmemu\src /I ..\romwbw_emu\src /D _CRT_SECURE_NO_WARNINGS test_emu.cpp ..\cpmemu\src\qkz80.cc ..\cpmemu\src\qkz80_errors.cc ..\cpmemu\src\qkz80_mem.cc ..\cpmemu\src\qkz80_reg_set.cc /Fo:obj\tests\emu\ /Fe:obj\tests\emu\test_emu.exe /link /SUBSYSTEM:CONSOLE
