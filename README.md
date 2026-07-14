# z80cpmw

Z80 CP/M emulator for Windows. A native Windows port of the RomWBW/HBIOS emulator.

## Download

- **Beta — recommended right now:**
  [z80cpmw-1.0.17-beta.msix](https://github.com/avwohl/z80cpmw/releases/download/v1.0.17-beta/z80cpmw-1.0.17-beta.msix)
  — download and double-click to install. Signed; installs side-by-side with the
  Store version and updates in place over any earlier beta. Fixes the silent crash
  on F5 ([#1](https://github.com/avwohl/z80cpmw/issues/1))
  and the Load/Save Profile crash ([#2](https://github.com/avwohl/z80cpmw/issues/2)),
  and adds terminal paste, scrollback, and a configurable keyboard map.
- **Microsoft Store:** search for **Z80CPM**. The Store currently has **v1.0.14**,
  which predates the fixes and features above; it will be updated once the beta
  settles. This README describes the beta.
- All releases: [github.com/avwohl/z80cpmw/releases](https://github.com/avwohl/z80cpmw/releases)

## Features

- Z80 CPU emulation with accurate timing
- RomWBW HBIOS emulation
- VT100-compatible terminal display (25x80) with scrollback (mouse wheel / Shift+PageUp)
- Support for CP/M, ZSDOS, and other operating systems
- Multiple ROM images included
- Disk image support (up to 64MB hd1k format)
- Configurable keyboard map for function/navigation keys (termcap-style)
- Mouse text selection with right-click Copy/Paste

## Building

Requirements:
- Visual Studio 2022 or later
- Windows SDK 10.0 or later

Open `z80cpmw.sln` in Visual Studio and build the solution.

## Usage

1. Launch z80cpmw.exe (on first run, a scrollable **Getting Started** help
   window opens automatically; you can reopen it any time with **F1**)
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

Standard keyboard input. Arrow keys, Home/End, Insert, PageUp/PageDown and the
function keys (F1–F12) send VT100/xterm escape sequences to CP/M. Because CP/M
is pure ASCII with no standard for these keys, every binding is configurable —
see **Configuration** below.

By default `F1` opens Help and `F5` / `Shift+F5` start/stop the emulator, so
those two keys are not passed to CP/M unless you enable them in the config.

### Mouse Copy/Paste

Drag to select text in the terminal, then right-click for **Copy** and
**Paste**. `Ctrl+C` / `Ctrl+V` are left untouched so they still reach CP/M as
`^C` / `^V`.

## Configuration

Settings are stored in `%LOCALAPPDATA%\z80cpmw\z80cpmw.json`, which you can edit
by hand. This includes the keyboard map (`keyboard.keys`, written as termcap-style
escape strings), the `f1ToCpm` / `f5ToCpm` toggles, fonts, ROM and disk
assignments. The keyboard map and a Getting Started guide are also viewable
in-app from **Help → Help Topics** (the **Getting Started** and **Configuration
File** topics, which work even offline).

See [docs/CONFIGURATION.md](docs/CONFIGURATION.md) for the full reference,
including the escape syntax, bindable key names, and the default bindings.

## Related Projects

This Windows build is the feature reference for the emulator family. The sibling
ports (iOS/macOS `ioscpm`, Android `cpmdroid`, Linux `romwbw_emu`) can use
[FEATURE_PARITY.md](FEATURE_PARITY.md) as the checklist of UX features to reach
parity, with pointers to the canonical implementation here.

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
- [uc80](https://github.com/avwohl/uc80) - ANSI C compiler targeting Z80 processor and CP/M 2.2 operating system
- [ucow](https://github.com/avwohl/ucow) - Unix/Linux Cowgol to Z80 compiler
- [um80_and_friends](https://github.com/avwohl/um80_and_friends) - Microsoft MACRO-80 compatible toolchain for Linux: assembler, linker, librarian, disassembler
- [upeepz80](https://github.com/avwohl/upeepz80) - Universal peephole optimizer for Z80 compilers
- [uplm80](https://github.com/avwohl/uplm80) - PL/M-80 compiler targeting Intel 8080 and Zilog Z80 assembly language

