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
network connection** - see [ROMs and Disk Images](#roms-and-disk-images).

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

## ROMs and Disk Images

**Nothing is bundled.** The package carries the executable, the wxWidgets and
VC++ runtime DLLs, and (in a build made with the optional `..\ioscpm` checkout)
the help text. It carries no ROM and no disk image.

Both come from the interface-v0 catalog in
[romwbw_disks](https://github.com/avwohl/romwbw_disks). The only content address
compiled into this application is the index that catalog starts from
(`z80cpmw/CatalogV0.cpp`); the help base URL comes out of that index at run
time, and the index names every published RomWBW release and points at that
release's own list of ROMs and disk images, so no download URL is ever assembled
here from a version number.

Everything fetched lands in the data folder and is measured against the size and
SHA-256 the catalog publishes: a download whose bytes do not match is deleted
rather than kept, and a ROM is refused outright if the catalog carries no
checksum for it, because fifteen banks of unknown bytes under a CPU is not a
risk worth the convenience.

Three things are yours to choose, all under **Emulator → Settings**:

- **Which RomWBW release** - the picker at the top of the **Disk Images** page.
  That list is the index: every release `romwbw_disks` publishes is offered, and
  **a new RomWBW release reaches you with no new build of this application**,
  exactly as a new ROM or a new disk image does. Development snapshots are the
  one exception, and they are not releases - see **Show development snapshots**
  below.
  This paragraph said the opposite until 2026-09-17, and the thing it described
  was real: the list was filtered through the emulator core's
  `emu_romwbw_release_supported()`, which answered from a compile-time list in
  `romwbw_emu/src/romwbw_pin.h`, so a 3.7.0 entry was dropped by any binary built
  before somebody added 3.7.0 there. romwbw_emu v1.44 deleted the function, the
  list and the header. The reasoning is in `romwbw_emu/DOWNSTREAM.md`: the
  release number is the pairing between a ROM and a disk image - which the guest
  enforces itself, by printing `*** WARNING: HBIOS/CBIOS Version Mismatch ***` -
  and not a property of the emulator. What the emulator depends on is the
  emulator-to-ROM interface, and that is versioned by the name of the catalog
  this app reads: `index-v0.json`. An interface change the core could not service
  would be published as `index-v1.json`, which this build would never open.
  What the release picker still owes you is a matched pair, and it says so: pick
  a release whose ROM is not the one in the banks and the note under the picker
  tells you Start will offer to fetch that release's ROM.
- **Show development snapshots** - the checkbox under the release picker, off by
  default. `romwbw_disks` publishes RomWBW development snapshots beside the
  releases; upstream has not released them, they are never the catalog's default,
  and their disk images pair only with their own ROM. Tick the box and they join
  the list, each marked *(development snapshot)*. Unticking it does **not** move a
  machine that is already running one: the release stays selected and stays in the
  picker, because switching release under mounted images is exactly what produces
  the guest's mismatch banner. The picker moves a machine; the checkbox only
  decides what the picker lists.
- **Which ROM** - the dropdown on the **Machine** page, filled from the selected
  release's ROMs. Publishing a new ROM in `romwbw_disks`, into a release the
  picker already offers, makes it selectable with no new release of this
  application.
- **Which catalog** - the *Catalog index* field under the release picker. Leave
  it empty for the catalog this build ships with, or point it at another
  `romwbw_disks` index to try a release before it is published.
  `ROMWBW_INDEX_URL` in the environment overrides it for one run and stores
  nothing; the field is disabled and says so while it is set. The order -
  environment, then setting, then built-in - matches `romwbw_emu`'s
  `romwbw-get`. Every catalog reads and writes the same data folder, so two
  catalogs publishing an image under one name share one file;
  [`docs/CONFIGURATION.md`](docs/CONFIGURATION.md) has the details.

### The first run

No ROM may be loaded before the catalog carrying its size and checksum has been
read, so a machine that has never reached the network has nothing it is allowed
to boot. Press **F5** on a fresh install and it fetches the catalog, downloads
the two default disk images (`hd1k_combo` and `hd1k_games`) and then the ROM
(about 512 KB); it starts when that ROM has arrived and matched. With no network
and nothing cached it says so and asks you to check the connection and press F5
again. After one success everything is cached, and later starts cost one small
catalog request and one checksum. If the catalog cannot be read later on, it
mounts whichever default disks are already cached and tries to start on those;
whether it gets there depends on the ROM gate, which needs a cached ROM for the
selected release. With neither, it says so instead of running empty banks.

A ROM placed by hand is searched for before the data folder's copy, in this
order: `<appDir>\roms\`, `<appDir>\`, `<appDir>\..\roms\`, then the data folder.
It is still used only after the catalog has been read and the file has matched
the published size and checksum.

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

### Boot Menu Keys

Every command is a line, so it needs Enter.

- `2` - boot the first hard disk, slice 0; `2.3` for slice 3
- `d` - list the devices
- `h` - the command set. On RomWBW 3.6.0 this lists the ROM applications too;
  on 3.5.1 they are under `l`, which 3.6.0 answers with `*** Invalid command`

Units 0 and 1 are the on-board RAM and ROM memory disks and carry no operating
system, so they boot nothing.

### Keyboard

Arrow keys, Home/End, Insert, PageUp/PageDown and the function keys (F1-F12)
send VT100/xterm escape sequences to CP/M. Because CP/M is pure ASCII with no
standard for these keys, every binding is configurable - see
[Configuration](#configuration).

By default `F1` opens Help and `F5` / `Shift+F5` start and stop the emulator, so
those two are not passed to CP/M unless you enable them in the config. Reset has
no shortcut by default: `Ctrl+R` is a character CP/M itself uses, so it goes to
the guest and Reset stays on the **Emulator** menu. Set `"ctrlRToCpm": false` to
claim `Ctrl+R` for Reset instead.

### Mouse Copy/Paste

Drag to select text in the terminal, then right-click for **Copy** and
**Paste**. `Ctrl+C` / `Ctrl+V` are left untouched so they still reach CP/M as
`^C` / `^V`.

### File Transfer (R8 / W8)

`R8 name` imports a file from the host into CP/M; `W8 name` exports one. On
Windows both take a **full path**, and `W8` prints the path the file really went
to - which on any MSIX install is the redirected `LocalCache` location, not the
one you typed:

```
R8 C:\Users\me\Desktop\getkey2.com
W8 REPORT.TXT C:\Users\me\Desktop\report.txt
```

A bare name (`W8 out.com`) goes to the app's data folder, whose real location
the app shows in **Emulator → Settings** (with an **Open Folder** button),
**Help → About**, and the boot banner.

Both utilities come from the disk catalog, not from this app, and only the
`hd1k_combo` image carries them - boot a single-OS image and there is no `R8` or
`W8` to run. [docs/FILE_TRANSFER.md](docs/FILE_TRANSFER.md) covers where
exported files land on the Store build and on the other ports.

## Configuration

Settings live in `z80cpmw.json`. Under MSIX - which both shipping channels are -
the `%LOCALAPPDATA%\z80cpmw\` write is redirected into the package's
`LocalCache`, which is why the app shows the resolved path and an **Open
Folder** button rather than printing a path you would have to translate.

The file is nested rather than flat. `rom` and `romwbwVersion` live under
`core`; `f1ToCpm`, `f5ToCpm`, `ctrlRToCpm` and the termcap-style `keys` map live
under `keyboard`; fonts and disk assignments have their own objects. Putting a
key at the top level does not work, but it is not silent either - the loader
compares the document against a reference and reports anything the reference
lacks as an unknown member.
`rom` holds the catalog's **id** (`emu_avw`, not a filename), because a filename
carries the release and would be forgotten the first time you switched releases;
an empty `rom` means "whichever ROM the catalog marks as its default". A file
written by an older build, holding `emu_avw.rom`, is converted to the id as it
is read.

Help topics are also viewable from **Help → Help Topics**. The status line at
the foot of that window names the copy you are reading - `downloaded`, `offline
copy` with the date, `bundled with the app`, `unavailable`, or `this session's
copy`, which means the window repainted from its fifteen-minute in-memory cache
and nothing was fetched.

See [docs/CONFIGURATION.md](docs/CONFIGURATION.md) for the full reference,
including the escape syntax, bindable key names and the default bindings.

## Building

**Requirements:**

- Visual Studio 18 or later (the project sets `PlatformToolset` to `v145`)
- Windows SDK 10.0 or later
- **wxWidgets 3.3 (x64)** through vcpkg. `z80cpmw.vcxproj` hard-codes the paths
  to `C:\temp\vcpkg\installed\x64-windows\`, so either install vcpkg there or
  edit `AdditionalIncludeDirectories` / `AdditionalLibraryDirectories`. Without
  it the build stops at `SettingsDialogWx.h(9): fatal error C1083: Cannot open
  include file: 'wx/wx.h'`.
- [`cpmemu`](https://github.com/avwohl/cpmemu) and
  [`romwbw_emu`](https://github.com/avwohl/romwbw_emu) checked out **next to**
  this one: the project compiles emulator sources directly from `..\cpmemu\src`
  and `..\romwbw_emu\src`.
- **Optional:** [`ioscpm`](https://github.com/avwohl/ioscpm), also next to this
  one. When present, `z80cpmw.rc` compiles the shared help assets from
  `..\ioscpm\release_assets` into the executable as `RCDATA`, so the online help
  topics can be read with no network and an empty cache. Without it the build
  succeeds and defines `NO_BUNDLED_HELP_ASSETS`; what is lost is only the
  offline floor for those topics. `/p:BundleHelpAssets=false` forces that build
  on a machine that does have the checkout.

Open `z80cpmw.sln` in Visual Studio and build the solution.
`tests\run_tests.bat` needs `..\cpmemu` and `..\romwbw_emu` for its last three
suites and stops without them; without `..\ioscpm` the help suite skips its
bundled-asset section and runs the rest.

Packaging is scripted: `packaging\scripts\build-msix.ps1` builds the unsigned
Store package, and `-Beta` builds and signs the sideload package instead.
`-SkipBuild` packages an existing `bin\Release` without rebuilding, checking the
binary's version against `z80cpmw/Version.h` and refusing a mismatch. See
[packaging/STORE_SUBMISSION.md](packaging/STORE_SUBMISSION.md) and
[docs/CODE_SIGNING.md](docs/CODE_SIGNING.md).

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
