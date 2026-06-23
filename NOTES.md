# z80cpmw Development Notes

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
