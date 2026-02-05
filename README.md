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

- [80un](https://github.com/avwohl/80un) - CP/M archive unpacker (LBR, ARC, Squeeze, Crunch, CrLZH)
- [cpmdroid](https://github.com/avwohl/cpmdroid) - CP/M emulator for Android
- [ioscpm](https://github.com/avwohl/ioscpm) - CP/M emulator for iOS/macOS
- [learn-ada-z80](https://github.com/avwohl/learn-ada-z80) - Ada programming examples for Z80/CP/M
- [uada80](https://github.com/avwohl/uada80) - Ada compiler for Z80/CP/M
- [uc80](https://github.com/avwohl/uc80) - C compiler for Z80/CP/M
- [um80_and_friends](https://github.com/avwohl/um80_and_friends) - MACRO-80 compatible assembler/linker toolchain
- [upeepz80](https://github.com/avwohl/upeepz80) - Peephole optimizer for Z80 assembly
- [uplm80](https://github.com/avwohl/uplm80) - PL/M-80 compiler for Z80

## License

GPL v3 License

CP/M operating system is licensed by Lineo for non-commercial use.
