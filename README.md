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

- [RomWBW](https://github.com/wwarthen/RomWBW) - The original RomWBW project by Wayne Warthen
- [cpmemu](https://github.com/avwohl/cpmemu) - Portable Z80 CPU emulator core
- [romwbw_emu](https://github.com/avwohl/romwbw_emu) - CLI emulator for macOS/Linux
- [ioscpm](https://github.com/avwohl/ioscpm) - iOS/macOS version
- [cpmdroid](https://github.com/avwohl/cpmdroid) - Android version

## License

GPL v3 License

CP/M operating system is licensed by Lineo for non-commercial use.
