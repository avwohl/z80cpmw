# z80cpmw

Z80 CP/M emulator for Windows. A native Windows port of the RomWBW/HBIOS emulator.

## Features

- Z80 CPU emulation with accurate timing
- RomWBW HBIOS emulation
- VT100-compatible terminal display (25x80)
- Support for CP/M, ZSDOS, and other operating systems
- Multiple ROM images included
- Disk image support (up to 64MB hd1k format)

## Building

Requirements:
- Visual Studio 2022 or later
- Windows SDK 10.0 or later

Open `z80cpmw.sln` in Visual Studio and build the solution.

## Usage

1. Launch z80cpmw.exe
2. Select a ROM from File > Select ROM (default: EMU AVW)
3. Optionally load disk images from File > Load Disk
4. Click Emulator > Start (or press F5)
5. At the RomWBW boot menu, press a number to boot an OS

### Boot Menu Keys

- `h` - Help
- `l` - List ROM applications
- `d` - List devices
- `0-9` - Boot from device

### Keyboard

Standard keyboard input. Arrow keys send VT100 escape sequences.
## Related Projects

- [80un](https://github.com/avwohl/80un) - Unpacker for CP/M compression and archive formats (LBR, ARC, squeeze, crunch, CrLZH)
- [cpmdroid](https://github.com/avwohl/cpmdroid) - Z80/CP/M emulator for Android with RomWBW HBIOS compatibility and VT100 terminal
- [cpmemu](https://github.com/avwohl/cpmemu) - CP/M 2.2 emulator with Z80/8080 CPU emulation and BDOS/BIOS translation to Unix filesystem
- [ioscpm](https://github.com/avwohl/ioscpm) - Z80/CP/M emulator for iOS and macOS with RomWBW HBIOS compatibility
- [learn-ada-z80](https://github.com/avwohl/learn-ada-z80) - Ada programming examples for the uada80 compiler targeting Z80/CP/M
- [mbasic](https://github.com/avwohl/mbasic) - Modern MBASIC 5.21 Interpreter & Compilers
- [mbasic2025](https://github.com/avwohl/mbasic2025) - MBASIC 5.21 source code reconstruction - byte-for-byte match with original binary
- [mbasicc](https://github.com/avwohl/mbasicc) - C++ implementation of MBASIC 5.21
- [mbasicc_web](https://github.com/avwohl/mbasicc_web) - WebAssembly MBASIC 5.21
- [mpm2](https://github.com/avwohl/mpm2) - MP/M II multi-user CP/M emulator with SSH terminal access and SFTP file transfer
- [romwbw_emu](https://github.com/avwohl/romwbw_emu) - Hardware-level Z80 emulator for RomWBW with 512KB ROM + 512KB RAM banking and HBIOS support
- [scelbal](https://github.com/avwohl/scelbal) - SCELBAL BASIC interpreter - 8008 to 8080 translation
- [uada80](https://github.com/avwohl/uada80) - Ada compiler targeting Z80 processor and CP/M 2.2 operating system
- [ucow](https://github.com/avwohl/ucow) - Unix/Linux Cowgol to Z80 compiler
- [um80_and_friends](https://github.com/avwohl/um80_and_friends) - Microsoft MACRO-80 compatible toolchain for Linux: assembler, linker, librarian, disassembler
- [upeepz80](https://github.com/avwohl/upeepz80) - Universal peephole optimizer for Z80 compilers
- [uplm80](https://github.com/avwohl/uplm80) - PL/M-80 compiler targeting Intel 8080 and Zilog Z80 assembly language

