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

The terminal is a fixed 25x80 grid with no scrollback, so a long startup banner
scrolled its first lines off-screen permanently. Fixed by:

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
- F1 (Help) and F5/Shift+F5 (Start/Stop) are application accelerators. The
  accelerator table is now built at runtime in `MainWindow::run` from
  `keyboard.f1ToCpm` / `keyboard.f5ToCpm`; setting either true omits that
  accelerator so the key reaches CP/M via the keymap instead.
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

### Current Implementation

W8/R8 are CP/M utilities that transfer files between CP/M and the host system using HBIOS extension traps (RST 8):

```
H_OPEN_R = 0xE1  ; Open host file for reading (DE=filename)
H_OPEN_W = 0xE2  ; Open host file for writing (DE=filename)
H_READ   = 0xE3  ; Read byte, returns in A
H_WRITE  = 0xE4  ; Write byte (E=byte)
H_CLOSE  = 0xE5  ; Close file (C=0 read, C=1 write)
```

Files are read/written to the data folder: `%LocalAppData%\z80cpmw\data\`

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
  z80cpmw.ini          - Settings file
  data\                - Disk images and file transfers
    hd1k_combo.img     - Downloaded disk images
    hd1k_games.img
    <files from W8>    - Exported files from CP/M
    <files for R8>     - Files to import to CP/M
```

This location is used because Microsoft Store apps cannot write to Program Files.

## Store App Compatibility

- App install directory is read-only
- All writable files go to LocalAppData
- ROMs are read from app install directory (read-only resources)

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
