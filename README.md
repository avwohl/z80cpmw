# z80cpmw

Z80 CP/M emulator for Windows. A native Windows port of the RomWBW/HBIOS
emulator.

## Download

- **Signed sideload build:** take the `.msix` from
  [the latest release](https://github.com/avwohl/z80cpmw/releases/latest).
  Download and double-click; the Azure Trusted Signing certificate chains to a
  Microsoft public root, so no developer mode and no certificate import are
  needed. (The link names no file on purpose: every package carries its version
  in its name, so there is no stable filename for
  `releases/latest/download/` to resolve.)
- **Microsoft Store:** search for **Z80CPM**. Microsoft signs it and it updates
  itself, which makes it the easiest way in.
- The two packages carry different publishers, so the sideload build installs
  **side-by-side** with a Store install rather than replacing it, and it updates
  in place over any earlier beta. Uninstall whichever you do not want.
- All releases: [github.com/avwohl/z80cpmw/releases](https://github.com/avwohl/z80cpmw/releases)
- What changed in each version: [CHANGELOG.md](CHANGELOG.md)

**Neither package ships a ROM or a disk image**, so **the first run needs a
network connection** - see [ROMs and Disk Images](docs/ROMS_AND_DISK_IMAGES.md).

## Features

- Z80 CPU emulation with RomWBW HBIOS emulation
- VT100/VT52-compatible terminal display (25x80) with scrollback (mouse wheel /
  Shift+PageUp)
- VT52 emulation, auto-detected from any VT52-exclusive sequence
- Scrolling region (DECSTBM), deferred autowrap (DECAWM), and answerback for
  cursor-position, status and identify queries
- Runs CP/M, ZSDOS and the other operating systems the catalog publishes
- ROMs and disk images fetched from the RomWBW catalog and checked against the
  size and SHA-256 it publishes - nothing is bundled in the package
- Four disk slots, hd1k format
- Configurable keyboard map for function/navigation keys (termcap-style)
- Mouse text selection with right-click Copy/Paste

## Usage

1. Launch `z80cpmw.exe`. On first run a scrollable **Getting Started** help
   window opens; reopen it any time with **F1**
2. **Emulator > Start** (F5). With no disks mounted this is also where the ROM
   and the two default disk images are fetched, so the first start needs a
   network connection and takes longer than the ones after it
3. At the RomWBW boot menu, type `2` and press Enter to boot the first hard
   disk

To change ROM or RomWBW release, open **Emulator > Settings**. **File > Load
Disk 0/1** mounts images into the first two units; **Settings > Machine** has
all four slots, each with its own Browse and New buttons.

## Documentation

- [ROMs and Disk Images](docs/ROMS_AND_DISK_IMAGES.md) - the RomWBW catalog, choosing a release and a ROM, pre-releases, the first run
- [Using z80cpmw](docs/USAGE.md) - boot menu keys, keyboard, mouse copy/paste, R8/W8 file transfer
- [Configuration](docs/CONFIGURATION.md) - `z80cpmw.json`, the keyboard map, escape syntax and default bindings
- [File Transfer](docs/FILE_TRANSFER.md) - where R8/W8 files go on each platform
- [Building](docs/BUILDING.md) - requirements, sibling checkouts, tests and MSIX packaging
- [Code Signing](docs/CODE_SIGNING.md) - how the sideload packages are signed
- [CHANGELOG](CHANGELOG.md) - what changed in each version

## Related Projects

This Windows build is the feature reference for the emulator family. The sibling
ports (iOS/macOS `ioscpm`, Android `cpmdroid`, Linux `romwbw_emu`) can use
[FEATURE_PARITY.md](FEATURE_PARITY.md) as the checklist of UX features to reach
parity, with pointers to the canonical implementation here.

- [80un](https://github.com/avwohl/80un) - Unpacker for the CP/M archive and compression formats LBR, ARC, squeeze, crunch, and CrLZH.
- [cpmdroid](https://github.com/avwohl/cpmdroid) - Z80/CP/M emulator for Android phones and tablets. It emulates the RomWBW HBIOS interface and a VT100 terminal.
- [cpmemu](https://github.com/avwohl/cpmemu) - Z80/CP/M emulator for Linux and Windows, with Z80 and 8080 CPU cores. It translates the BDOS and BIOS calls of CP/M 2.2 programs to the host file system.
- [ioscpm](https://github.com/avwohl/ioscpm) - Z80/CP/M emulator for iOS and macOS. It emulates the RomWBW HBIOS interface and runs CP/M 2.2 and CP/M 3.
- [learn-ada-z80](https://github.com/avwohl/learn-ada-z80) - Collection of more than 90 Ada example programs for uada80, the Ada compiler for the Z80 processor and CP/M.
- [mbasic](https://github.com/avwohl/mbasic) - Python interpreter for MBASIC 5.21, the Microsoft BASIC-80 for CP/M. Two compiler backends compile the programs to CP/M .COM files or to JavaScript.
- [mbasic2025](https://github.com/avwohl/mbasic2025) - Reconstruction of the lost source code of MBASIC 5.21, the Microsoft BASIC-80 for CP/M. The MACRO-80 source code assembles to a binary that matches mbasic.com byte for byte.
- [mbasicc](https://github.com/avwohl/mbasicc) - C++17 interpreter for MBASIC 5.21, the Microsoft BASIC-80 for CP/M. It runs on Linux and macOS.
- [mbasicc_web](https://github.com/avwohl/mbasicc_web) - Web browser interpreter for MBASIC 5.21, the Microsoft BASIC-80 for CP/M. Emscripten compiles the mbasicc interpreter to WebAssembly.
- [mpm2](https://github.com/avwohl/mpm2) - Z80 emulator for MP/M II, the multi-user CP/M operating system. Users connect over SSH, and SFTP clients transfer files.
- [romwbw_emu](https://github.com/avwohl/romwbw_emu) - Hardware-level Z80/CP/M emulator for Linux and macOS. It emulates the RomWBW HBIOS interface and switches banks in 512 KB of ROM and 512 KB of RAM.
- [scelbal](https://github.com/avwohl/scelbal) - Floating-point BASIC interpreter for the 8080 processor and CP/M. A translator converts the original 8008 source code to 8080 source code.
- [uada80](https://github.com/avwohl/uada80) - Ada compiler for the Z80 processor and CP/M 2.2. It compiles a subset of Ada 2012 to CP/M .COM files.
- [uc80](https://github.com/avwohl/uc80) - C compiler for the Z80 processor and CP/M. It optimizes for small code size.
- [ucow](https://github.com/avwohl/ucow) - Cowgol compiler for the Z80 processor and CP/M. It runs on Linux in Python.
- [um80_and_friends](https://github.com/avwohl/um80_and_friends) - Linux toolchain that is compatible with Microsoft MACRO-80. It has an assembler, a linker, a librarian, and a disassembler.
- [upeepz80](https://github.com/avwohl/upeepz80) - Peephole optimizer for Z80 compilers that write lowercase Z80 assembly language. It shortens jumps to jr, builds djnz loops, and removes dead stores.
- [uplm80](https://github.com/avwohl/uplm80) - PL/M-80 compiler for the Z80 processor and CP/M. It writes Intel 8080 and Zilog Z80 assembly language.


## See Also

- [RomWBW](https://github.com/wwarthen/RomWBW) - The original RomWBW project by Wayne Warthen
