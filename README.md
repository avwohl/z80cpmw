# z80cpmw

Z80 CP/M emulator for Windows. A native Windows port of the RomWBW/HBIOS emulator.

## Download

- **Microsoft Store — recommended:** search for **Z80CPM**. The Store has
  **v1.0.22**, released 2026-08-23. Microsoft signs it and it updates itself, so
  this is the easiest way in for most people.
- **Signed sideload beta:**
  [z80cpmw-1.0.22-beta.msix](https://github.com/avwohl/z80cpmw/releases/download/v1.0.22-beta/z80cpmw-1.0.22-beta.msix)
  — for machines that cannot install from the Store. This is the **same binary**
  as Store v1.0.22, repackaged and signed for sideloading. Download and
  double-click; the Azure Trusted Signing certificate chains to a Microsoft
  public root, so no developer mode and no certificate import are needed.
- The two packages carry different publishers, so the sideload build installs
  **side-by-side** with a Store install rather than replacing it, and it updates
  in place over any earlier beta. Uninstall whichever you do not want.
- All releases: [github.com/avwohl/z80cpmw/releases](https://github.com/avwohl/z80cpmw/releases)
- What changed in each version: [CHANGELOG.md](CHANGELOG.md)

## Features

- Z80 CPU emulation with accurate timing
- RomWBW HBIOS emulation
- VT100/VT52-compatible terminal display (25x80) with scrollback (mouse wheel / Shift+PageUp)
- VT52 emulation, auto-detected from any VT52-exclusive sequence
- Scrolling region (DECSTBM), deferred autowrap (DECAWM), and answerback for
  cursor-position, status and identify queries
- Support for CP/M, ZSDOS, and other operating systems
- Multiple ROM images included
- Disk image support (up to 64MB hd1k format)
- Configurable keyboard map for function/navigation keys (termcap-style)
- Mouse text selection with right-click Copy/Paste

## Building

Requirements:
- Visual Studio 18 or later (the project sets `PlatformToolset` to `v145`)
- Windows SDK 10.0 or later
- **wxWidgets 3.3 (x64)**, installed through vcpkg. `z80cpmw.vcxproj` hard-codes
  the include and library paths to `C:\temp\vcpkg\installed\x64-windows\`, so
  either install vcpkg there or edit `AdditionalIncludeDirectories` /
  `AdditionalLibraryDirectories` to match your own location. Without it the
  build stops at `SettingsDialogWx.h(9): fatal error C1083: Cannot open include
  file: 'wx/wx.h'`.
- The sibling repositories [`cpmemu`](https://github.com/avwohl/cpmemu) and
  [`romwbw_emu`](https://github.com/avwohl/romwbw_emu) checked out **next to**
  this one: the project compiles emulator sources directly from
  `..\cpmemu\src` and `..\romwbw_emu\src`.
- **Optional:** the sibling repository
  [`ioscpm`](https://github.com/avwohl/ioscpm), also next to this one. When it
  is there, `z80cpmw.rc` compiles the eight shared help assets in
  `..\ioscpm\release_assets` into the executable as `RCDATA`, so the seven
  online help topics can still be read with no network and an empty cache. It is
  a build input only — nothing links against it, and the text it carries is
  whatever that checkout holds at build time.

  Without it the build succeeds and simply ships no compiled-in help:
  `z80cpmw.vcxproj` tests for
  `..\ioscpm\release_assets\help_index.json` and, when it is missing, defines
  `NO_BUNDLED_HELP_ASSETS`, which guards those eight `RCDATA` lines out. What is
  lost is the offline floor for those seven topics — a reader who has never
  successfully downloaded one, and has nothing in the on-disk cache, gets *"This
  topic could not be downloaded"* instead of the bundled text. Everything else
  is unchanged: the topics still download, still cache to disk, and the two
  written-in topics (**Getting Started** and **Configuration File**) are not
  affected at all. Pass `/p:BundleHelpAssets=false` to force that build on a
  machine that does have the checkout.

`tests\run_tests.bat` needs `..\cpmemu` and `..\romwbw_emu` for its last three
suites and stops without them. `..\ioscpm` it treats differently, for the same
reason the build does: the help suite skips the one section that checks the
bundled assets and runs the rest.

Open `z80cpmw.sln` in Visual Studio and build the solution.

Packaging is scripted: `packaging\scripts\build-msix.ps1` builds the unsigned
Store package, and `-Beta` builds and signs the sideload package instead. Pass
`-SkipBuild` to package an existing `bin\Release` without rebuilding — the
script checks the binary's version against `z80cpmw/Version.h` and refuses a
mismatch. See [packaging/STORE_SUBMISSION.md](packaging/STORE_SUBMISSION.md) and
[docs/CODE_SIGNING.md](docs/CODE_SIGNING.md).

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
Reset has no shortcut by default: `Ctrl+R` is a character CP/M itself uses, so
it goes to the guest and Reset stays on the **Emulator** menu. Set
`"ctrlRToCpm": false` to claim `Ctrl+R` for Reset instead.

### Mouse Copy/Paste

Drag to select text in the terminal, then right-click for **Copy** and
**Paste**. `Ctrl+C` / `Ctrl+V` are left untouched so they still reach CP/M as
`^C` / `^V`.

### File Transfer (R8 / W8)

`W8 name` exports a file from CP/M to the host; `R8 name` imports one. On
Windows, `R8` takes a **full path** and reads exactly that file (even on the
Store build):

```
R8 C:\Users\me\Desktop\getkey2.com
```

`W8` takes one too, and prints the path the file really went to — which on any
MSIX install is the redirected `LocalCache` location, not the one you typed:

```
W8 REPORT.TXT C:\Users\me\Desktop\report.txt
```

Both utilities come from the disk catalog, not from this app: the images are
published in the [ioscpm release area](https://github.com/avwohl/ioscpm/releases)
and pinned by `RELEASE_TAG` in `DiskCatalog.cpp`. Nothing is bundled in the
installer, so the `R8` and `W8` you get are whichever the pinned release carries.

A bare name (`W8 out.com`) goes to the app's data folder — whose real location the
app shows in *Emulator → Settings* (with an **Open Folder** button), *Help → About*,
and the boot banner. For where exported files land on the Store build and on the
macOS/iOS/Android ports — and how to find them — see
[docs/FILE_TRANSFER.md](docs/FILE_TRANSFER.md).

## Configuration

Settings are stored in `%LOCALAPPDATA%\z80cpmw\z80cpmw.json`, which you can edit
by hand. This includes the keyboard map (`keyboard.keys`, written as termcap-style
escape strings), the `f1ToCpm` / `f5ToCpm` / `ctrlRToCpm` toggles, fonts, ROM and disk
assignments. The keyboard map and a Getting Started guide are also viewable
in-app from **Help → Help Topics**, and every topic there works offline in a
build made with the optional `..\ioscpm` checkout: the **Getting Started** and
**Configuration File** topics are written into the app, and the seven guides
that normally come from the network fall back to a downloaded copy on disk and
then to the copy compiled in from `..\ioscpm\release_assets` (see
[Building](#building) for what a build without that checkout loses).

The status line at the foot of the help window names the copy you are reading:
`downloaded`, `offline copy` (with the date it was saved), `bundled with the
app`, or `unavailable`. There is a fifth answer, **`this session's copy`**, and
it means something different from the others — the help window keeps a topic in
memory for fifteen minutes after it is first shown, and re-reading it inside
that window repaints from memory without touching the network or the disk. That
in-memory entry does not record which of the four the text originally came from,
so the status line does not guess — it tells you only that nothing was fetched
just now.

See [docs/CONFIGURATION.md](docs/CONFIGURATION.md) for the full reference,
including the escape syntax, bindable key names, and the default bindings.

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

