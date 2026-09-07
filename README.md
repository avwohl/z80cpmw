# z80cpmw

Z80 CP/M emulator for Windows. A native Windows port of the RomWBW/HBIOS emulator.

## Download

- **Signed sideload build — newest:**
  [z80cpmw-1.0.28-beta.msix](https://github.com/avwohl/z80cpmw/releases/latest/download/z80cpmw-1.0.28-beta.msix)
  — the release GitHub marks **Latest**. Download and double-click; the Azure
  Trusted Signing certificate chains to a Microsoft public root, so no developer
  mode and no certificate import are needed. This is the first build that ships
  **no ROM and no disk image**: both come from the RomWBW catalog and are checked
  against the size and SHA-256 it publishes, so **the first run needs a network
  connection**.
- **Microsoft Store:** search for **Z80CPM**. Microsoft signs it and it updates
  itself, which makes it the easiest way in — but it is currently **behind**:
  the Store serves **1.0.25**, measured with `tools/check-store-version.sh` on
  2026-09-07, and **1.0.29** (the same code as the build above) is packaged and
  waiting on submission.
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
- ROMs and disk images fetched from the RomWBW catalog and checked against the
  size and SHA-256 it publishes — nothing is bundled in the package
- Disk image support (up to 64MB hd1k format)
- Configurable keyboard map for function/navigation keys (termcap-style)
- Mouse text selection with right-click Copy/Paste

## ROMs and Disk Images

**Nothing is bundled.** The package carries the executable, the app-local VC++
runtime and — in a build made with the optional `..\ioscpm` checkout — the help
text. It carries no ROM and no disk image. Both come from the interface-v0
catalog in [romwbw_disks](https://github.com/avwohl/romwbw_disks), and the only
content address compiled into this application is the index that catalog starts
from (the in-app help has its own, and the crash dialog links to the issue
tracker)
(`z80cpmw/CatalogV0.cpp`); the index names every published RomWBW release and
points at that release's own list of ROMs and disk images, so no download URL is
ever assembled here from a version number. Everything fetched lands in the data
folder and is measured against the size and SHA-256 the catalog publishes: a
download whose bytes do not match is deleted rather than kept, and a ROM is
refused outright if the catalog carries no checksum for it, because fifteen
banks of unknown bytes under a CPU is not a risk worth the convenience.

Two things are yours to choose, both under **Emulator → Settings**:

- **Which RomWBW release** — the *RomWBW release* picker at the top of the
  **Disk Images** page. That list is not compiled in either: it is the index,
  filtered to the releases the emulator core says it can boot, so a release the
  core has never been checked against is not offered.
- **Which ROM** — the *ROM* dropdown on the **Machine** page, filled from the
  selected release's ROMs. Publishing a new ROM in `romwbw_disks` therefore
  makes it selectable with no new release of this application.

**The first run needs a network connection**, and that is the cost of shipping
no ROM. No ROM may be loaded before the catalog carrying its size and checksum
has been read, so a machine that has never reached the network has nothing it is
allowed to boot. Press **F5** on a fresh install and it fetches the catalog,
downloads the two default disk images (`hd1k_combo` and `hd1k_games`) and then
offers the ROM (about 512 KB); it starts when that ROM has arrived and matched.
If the network is not there it says so and asks you to check the connection and
press F5 again — it never boots unverified bytes. After one success everything
is cached in the data folder, and later starts cost one small catalog request
and one checksum.

A ROM you place in the data folder, or beside `z80cpmw.exe`, under the filename
the catalog gives it is used instead of downloading it again — but still only
after the catalog has been read and the file has matched the published size and
checksum.

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
2. Click Emulator > Start (or press F5). With no disks mounted this is also
   where the ROM and the two default disk images are fetched, so the very first
   start needs a network connection and takes longer than the ones after it —
   see [ROMs and Disk Images](#roms-and-disk-images)
3. At the RomWBW boot menu, press a number to boot an OS

To run a different ROM, or a different RomWBW release, open **Emulator >
Settings**; to mount images of your own, use **File > Load Disk 0/1**.

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

Both utilities come from the disk catalog, not from this app: they are on the
images published in [romwbw_disks](https://github.com/avwohl/romwbw_disks).
There is no pinned release tag in this application any more — the only
compiled-in address for content is the index — so which images you get follows
from the
RomWBW release selected in **Emulator > Settings > Disk Images**. Nothing is
bundled in the installer, so the `R8` and `W8` you get are whichever that
release's catalog carries.

A bare name (`W8 out.com`) goes to the app's data folder — whose real location the
app shows in *Emulator → Settings* (with an **Open Folder** button), *Help → About*,
and the boot banner. For where exported files land on the Store build and on the
macOS/iOS/Android ports — and how to find them — see
[docs/FILE_TRANSFER.md](docs/FILE_TRANSFER.md).

## Configuration

Settings are stored in `%LOCALAPPDATA%\z80cpmw\z80cpmw.json`, which you can edit
by hand. This includes the keyboard map (`keyboard.keys`, written as termcap-style
escape strings), the `f1ToCpm` / `f5ToCpm` / `ctrlRToCpm` toggles, fonts, ROM and disk
assignments. `rom` holds the catalog's **id** for a ROM — `emu_avw`, not a
filename — because a filename carries the release (`emu_avw-v0-3.5.1.rom` and
`emu_avw-v0-3.6.0.rom` are one choice) and would be forgotten the first time you
switched releases; `romwbwVersion` holds the release itself, and an empty `rom`
means "whichever ROM the catalog marks as its default". A file written by an
older build, holding `emu_avw.rom`, is converted to the id as it is read.

The keyboard map and a Getting Started guide are also viewable
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

