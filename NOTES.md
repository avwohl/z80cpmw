# z80cpmw Development Notes

## Auto-size Window to Terminal on Font Change (June 2026)

`MainWindow::resizeWindowToTerminal` sizes the window so the client area exactly
fits the 80x25 grid at the terminal's current char metrics (plus the status
bar), using AdjustWindowRectExForDpi for the frame/menu so it is correct on
high-DPI displays. It keeps the top-left corner, clamps to the monitor work
area, and skips maximized windows. Called from `onViewFontSize` (so changing the
font resizes the window to match) and from `show()` when there is no saved
placement (so the default window fits the configured font/DPI instead of the old
hard-coded ~820px guess, which was too small on scaled displays). A saved
placement still wins over the auto-size default.

## Main Window Placement Persistence (June 2026)

The main window reopens at its last position/size. `MainWindow::saveWindowPlacement`
(called from WM_CLOSE, which File > Exit also routes through) stores
`rcNormalPosition` + maximized state from GetWindowPlacement into
`config.window`, plus the bounds of the monitor it was on (MonitorFromWindow).
`restoreWindowPlacement` (from `show()`) reapplies it via SetWindowPlacement, but
first discards the saved spot if it no longer lands on any monitor
(MonitorFromRect == null) or the monitor's bounds changed (display unplugged,
rearranged, or a resolution change) - in which case the default placement is
used. Placement is stored in physical pixels and round-trips correctly under
per-monitor DPI-v2 (verified stable across save/restore/save).

## Startup Instructions Moved to Scrollable Help (June 2026)

> **Update (2026-08):** the terminal gained scrollback in 1.0.16-beta — 1000
> lines by default, scrolled with the mouse wheel or Shift+PageUp, and changed
> or turned off through `display.scrollbackLines` (Emulator > Settings). The
> short banner and the bundled Help topics below stayed as they are.

The terminal is a fixed 25x80 grid and had no scrollback at the time, so a long
startup banner scrolled its first lines off-screen permanently. Fixed by:

- Shrinking the terminal banner (MainWindow::showStartupInstructions) to a few
  lines that fit, pointing at F1 / Help for the full guide.
- Adding two bundled, scrollable Help topics served from the app with no network
  (HelpWindow: `help_topics::GettingStarted`, `help_topics::Configuration`).
  They are seeded into the topic list in WM_CREATE (before the async index
  fetch) so they appear instantly and work offline.
- `HelpWindow::show(parent, topicId)` / `ShowHelpWindow(parent, topicId)` can
  open straight to a topic; the help window is now DPI-scaled and centered
  rather than a fixed 800x600 (which was tiny on high-DPI displays).
- First-run auto-open: MainWindow posts WM_APP_SHOW_WELCOME after the window is
  shown; the handler opens Getting Started once and sets `core.welcomeShown` in
  z80cpmw.json so it does not reopen on later launches.

## Configurable Keyboard Map ("termcap in reverse") (June 2026)

CP/M is pure ASCII and has no native function/navigation keys; each CP/M
terminal historically defined its own escape sequences, so there is no single
standard. `Keymap.h` maps Windows virtual keys to byte sequences using
termcap-style escape strings (`\E`=ESC, `^X`=ctrl, `\NNN`=octal, etc.) so a
binding can be copied straight from a termcap/terminfo entry.

- Defaults follow the VT220/xterm convention (and match the arrow-key sequences
  the terminal already emitted): Insert `ESC[2~`, PageUp `ESC[5~`, PageDown
  `ESC[6~`, F1-F4 `ESC O P/Q/R/S`, F5-F12 `ESC[15~..ESC[24~`, Delete `0x7F`.
- Bindings live in `z80cpmw.json` under `keyboard.keys` (written out on load so
  they are visible/editable). Missing names fall back to built-in defaults; an
  empty value unbinds a key.
- `TerminalView::handleKeyDown` resolves special keys through `keymap::KeyMap`;
  printable keys still arrive via `WM_CHAR`.
- F1 (Help), F5/Shift+F5 (Start/Stop) and Ctrl+R (Reset) are application
  accelerators. The table is built at runtime by
  `MainWindow::rebuildAccelerators`, from `keyboard.f1ToCpm` /
  `keyboard.f5ToCpm` / `keyboard.ctrlRToCpm`; releasing a key omits that
  accelerator so the key reaches CP/M instead. `applyConfig` rebuilds the table
  and calls `updateMenuAccelHints`, so a profile that releases a key takes
  effect without a restart and the menu never advertises a dead shortcut.
  There is no `ACCELERATORS` resource in `z80cpmw.rc`; a static one used to sit
  there and was never loaded.
- The three flags are not symmetrical on purpose. F1/F5 default to the app
  because CP/M has no function keys. `ctrlRToCpm` defaults to **true** because
  `^R` (0x12) is ASCII CP/M really reads - the CCP retypes the line with it and
  WordStar-family editors bind it - and because `TranslateAccelerator` eats the
  whole keystroke, so no `WM_CHAR` is generated for `TerminalView::handleChar`.
  A released Ctrl+R needs no `Keymap.h` entry: it arrives as `WM_CHAR` 0x12.
- F10 normally activates the menu bar (arrives as `WM_SYSKEYDOWN`); it is
  intercepted in `TerminalView` when bound so it can be delivered to CP/M.

## Mouse Selection and Clipboard (June 2026)

`TerminalView` supports drag-select + right-click Copy/Paste. Ctrl+C/Ctrl+V are
deliberately left untouched so they still reach CP/M as `^C`/`^V`.

- Stream (wrapping) selection over `m_cells`, highlighted by swapping fg/bg in
  `paint()`. `SetCapture`/`ReleaseCapture` around the drag; `WM_CAPTURECHANGED`
  guards against capture theft.
- Right-click (`WM_CONTEXTMENU`) shows a Copy/Paste popup. Copy trims trailing
  spaces per line, joins with CRLF, sets `CF_UNICODETEXT`. Paste maps CRLF/LF to
  CR (`0x0D`) and feeds ASCII bytes through the key callback.
- Paste is greyed/blocked when the emulator is not running (input callback only
  delivers while running) via `setInputReadyCallback`.

## W8/R8 Host File Transfer (December 2024)

> **Update (2026-07):** the implementation since gained (1) **absolute/UNC/rooted
> path support** — such paths are written verbatim, even under full-trust MSIX —
> with bare names still going to the data folder; and (2) a **resolved-path
> display** (`emu_io_get_data_folder_display` → `GetFinalPathNameByHandle`) so
> About/Settings/boot-banner show the real folder even when the Store build
> redirects `%LOCALAPPDATA%` into the package `LocalCache`. The user-facing,
> cross-platform writeup (Windows/macOS/iOS/Android) is
> [`docs/FILE_TRANSFER.md`](docs/FILE_TRANSFER.md). The global-state / MP/M note
> below still stands.

### Current Implementation

W8/R8 are CP/M utilities that transfer files between CP/M and the host system using HBIOS extension traps (RST 8):

```
H_OPEN_R = 0xE1  ; Open host file for reading (DE=filename)
H_OPEN_W = 0xE2  ; Open host file for writing (DE=filename)
H_READ   = 0xE3  ; Read byte, returns in A
H_WRITE  = 0xE4  ; Write byte (E=byte)
H_CLOSE  = 0xE5  ; Close file (C=0 read, C=1 write)
```

Bare filenames are read/written in the data folder: `%LocalAppData%\z80cpmw\data\`
(absolute, UNC and rooted paths are used verbatim — see the update above).

### MP/M2 Limitation

The current implementation uses **global state** for file transfers:
- Only one file transfer can be active at a time
- In MP/M2 (multi-user), concurrent W8/R8 from different users would conflict

### Future Options (if needed)

1. **Add handles to protocol** - H_OPEN returns handle (0-3), H_WRITE/H_CLOSE take handle
2. **Serialize access** - Return error if transfer in progress
3. **Per-process state** - Track by MP/M process ID

### Current Workaround

Users can use **XMODEM** for file transfers in MP/M2 scenarios. The XMODEM protocol works through the terminal and doesn't require host-side file access.

## Data Directory Structure

All user data is stored in `%LocalAppData%\z80cpmw\`:

```
%LocalAppData%\z80cpmw\
  z80cpmw.json               - Settings file (a legacy z80cpmw.ini is migrated
                               once, then renamed z80cpmw.ini.bak)
  profiles\                  - Saved configuration profiles
  data\                      - ROMs, disk images and file transfers
    emu_avw-v0-3.6.0.rom     - Downloaded ROM
    hd1k_combo-v0-3.6.0.img  - Downloaded disk images
    hd1k_combo-v0-3.5.1.img
    disk_ledger.json         - The catalog sha256 each image was verified
                               against, plus the size/mtime that saves
                               re-hashing it on the next launch
    <files from W8>          - Exported files from CP/M
    <files for R8>           - Files to import to CP/M
```

This location is used because Microsoft Store apps cannot write to Program Files.

Every downloaded name carries the catalog interface and the RomWBW release the
file was built for - `hd1k_combo.img` became `hd1k_combo-v0-3.5.1.img` when the
catalog moved from `avwohl/ioscpm` to `avwohl/romwbw_disks`. That is not
cosmetic: a 3.5.1 disk booted against a 3.6.0 ROM makes the guest CBIOS print
`*** WARNING: HBIOS/CBIOS Version Mismatch ***`, so the two generations have to
sit in one folder without either overwriting the other, and only the filename
can keep them apart (`DiskMigrationV0.h`). Images already on disk under the
pre-v0 names are moved onto the v0 ones once, by
`DiskCatalog::migrateFilesToInterfaceV0`: renamed rather than copied, so the
ledger's cached (size, mtime) measurement survives and nobody re-hashes 210 MB
of images for nothing; driven by the fixed list of twenty legacy names in
`DiskMigrationV0.cpp` rather than by a pattern over the folder, because R8 and
W8 put the user's own files in this same directory and a pattern would rename
one of those; and deleting nothing, so where a file already sits under the v0
name the pre-v0 one is left exactly where it is.

The ROM is in this folder too, and since 2026-09-07 this is the only place a
downloaded one goes - no package ships a ROM any more.
`DiskCatalog::downloadRomInto` writes the catalog's `<id>-v0-<ver>.rom` to a
`.new` beside the final name and moves it on only after the size and sha256
match, and `MainWindow::loadCatalogRomForStart` checks both again on every
start - not only after a download - before the bytes reach the core. Which ROM
that is comes from the release catalog's `roms[]` and the user's choice on the
Machine page of Emulator > Settings; `core.rom` in z80cpmw.json holds the
catalog **id** (`emu_avw`), never a filename, because a filename carries the
release and a stored one would be forgotten by the first switch between 3.5.1
and 3.6.0. `Config.cpp`'s `from_json` converts the two filenames released builds
used to write through `diskv0::romIdForStoredName`, and leaves the field empty
for anything else - "no preference", which lets the catalog's `default: true`
entry decide.

## Store App Compatibility

- App install directory is read-only
- All writable files go to LocalAppData
- ROMs and disk images are downloaded into the data folder; the package carries
  neither

That last line used to read "ROMs are read from app install directory
(read-only resources)", and it was true until 2026-09-07, when
`roms\emu_avw.rom`, `roms\emu_romwbw.rom` and `roms\SBC_simh_std.rom` were
deleted from the tree. Nothing stages or ships a ROM now: the Debug
configuration of `z80cpmw.vcxproj` has no post-build event at all and the
Release one copies only the app-local CRT DLLs, `build-msix.ps1` has no `roms\`
staging directory to package, and the two `File` lines and the
`SetOutPath $INSTDIR\roms` are gone from `z80cpmw.nsi` - whose UNINSTALLER still
deletes all three names, to clear out installs made before this.

`MainWindow::findResourceFile` still looks in `<app>\roms\`, `<app>\` and
`<app>\..\roms\` before the data folder, so a ROM a user drops beside the
executable by hand still wins - which also means a leftover `bin\Release\roms`
from a build made before the deletion is still searched, and should be deleted
by hand. Winning is narrower than it sounds: `loadCatalogRomForStart` asks
`findResourceFile` for the catalog entry's own filename and then runs
`DiskCatalog::verifyRom` on whatever came back, so a hand-placed file is loaded
only when it is named `<id>-v0-<ver>.rom` and is the published bytes. Those
directories are read-only under the Store, which is why nothing can be added to
them at run time and why a downloaded ROM has nowhere else to land.

The cost of shipping no ROM, which belongs here rather than only in a release
note: a first launch with no network can no longer start the machine. Every ROM
is now checked against the size and sha256 that only the catalog carries, so a
machine that has never reached the network holds no ROM it is allowed to load.
It says so and offers to fetch it (`MainWindow::offerRomChoice`) rather than
booting unverified bytes or another release's ROM, and `loadDefaultROM()`
survives only to put that notice on the screen at startup - `startEmulator()`
clears the terminal, so the notice is the one thing that reaches the screen the
user is looking at.

## Unified RAM Bank Initialization (January 2026)

### For iOS/Mac Port

romwbw_emu commit b162fe9 unified the two independent RAM bank initialization systems into one.

**The Problem:** Previously there were two paths that could initialize RAM banks:
1. Port I/O path - via `initializeRamBankIfNeeded()` delegate method
2. SYSSETBNK path - via HBIOS function 0xF1 in hbios_dispatch.cc

Each had its own `initialized_ram_banks` bitmap, leading to potential double-initialization
and the SYSSETBNK path was missing the CBIOS page zero stamp at 0x40-0x55.

**The Fix:** `HBIOSDispatch` now owns the single bitmap and exposes it via:
```cpp
uint16_t* getInitializedBanksBitmap() { return &initialized_ram_banks; }
```

**What to Do for iOS/Mac:**

1. **Remove** any local `initialized_ram_banks` variable from your emulator class

2. **Update** `initializeRamBankIfNeeded()` to use the shared bitmap:
```cpp
// BEFORE:
void initializeRamBankIfNeeded(uint8_t bank) override {
    emu_init_ram_bank(&memory, bank, &initialized_ram_banks);
}

// AFTER:
void initializeRamBankIfNeeded(uint8_t bank) override {
    emu_init_ram_bank(&memory, bank, hbios.getInitializedBanksBitmap());
}
```

3. **Update** any places that reset the bitmap (reset callbacks, ROM loading, etc.):
```cpp
// BEFORE:
initialized_ram_banks = 0;

// AFTER:
*hbios.getInitializedBanksBitmap() = 0;
```

4. **Pull** the updated `hbios_dispatch.h` and `hbios_dispatch.cc` from romwbw_emu

**Benefits:**
- Single bitmap tracks all RAM bank initialization
- No redundant re-initialization
- CBIOS page zero stamp (0x40-0x55) is always installed correctly
- ASSIGN and MODE commands now work via SYSSETBNK path
