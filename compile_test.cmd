@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d C:\temp\src\z80cpmw
cl /nologo /EHsc /W3 /O2 /I z80cpmw /I z80cpmw/Core /D _CRT_SECURE_NO_WARNINGS test_emu.cpp z80cpmw/Core/qkz80.cc z80cpmw/Core/qkz80_errors.cc z80cpmw/Core/qkz80_mem.cc z80cpmw/Core/qkz80_reg_set.cc /Fe:test_emu.exe /link /SUBSYSTEM:CONSOLE
