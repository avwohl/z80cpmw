# Feature Parity Guide — z80cpmw (Windows) → sibling ports

**Purpose.** The Windows front-end (`z80cpmw`) currently has the richest UX of the
RomWBW/CP/M emulator family. This document is the canonical checklist of those
features so the other ports can reach parity. Point a coding agent in a sibling
repo at this file (raw URL works) and have it implement the gaps against the
referenced z80cpmw source.

Last reviewed: **2026-07-21** against each repo's public README/CHANGELOG plus
spot code searches. Port-status columns are best-effort — **verify against the
port's current source** before acting; absence of a mention is not proof of
absence.

## The family

| Platform | Repo | Lang | Role |
| --- | --- | --- | --- |
| Windows | [`z80cpmw`](https://github.com/avwohl/z80cpmw) | C++ | GUI front-end (this repo — the parity reference) |
| iOS / macOS | [`ioscpm`](https://github.com/avwohl/ioscpm) | Swift | GUI front-end. Also **hosts the shared disk-image + help release assets** (`disks.xml`, `hd1k_*.img`, `help_index.json`). |
| Android | [`cpmdroid`](https://github.com/avwohl/cpmdroid) | Kotlin | GUI front-end |
| Linux / Web | [`romwbw_emu`](https://github.com/avwohl/romwbw_emu) | C++ | **Shared emulator core** + CLI (deb/rpm) + browser/WASM frontend (`web/`). CLI terminal UX is delegated to the host terminal; the web build has its own xterm.js UI. |

`cpmemu` is a *different* family (BDOS/BIOS-to-Unix translation, not RomWBW HBIOS
hardware emulation) and is out of scope here.

**Two kinds of parity, two docs:**
- **Core-engine correctness** (Z80/HBIOS/banked-memory/shadow-RAM): owned by the
  shared core — see `romwbw_emu`'s `DOWNSTREAM.md`. Not covered here.
- **Front-end UX / features** (this document): the user-facing capabilities the
  Windows app grew that the others have not yet.

---

## Feature catalog

Each entry: what it does · behaviour spec · where it lives in z80cpmw · config/keys
· how it maps to other platforms.

### 1. Configurable keyboard map (termcap-style)  — *highest-value gap; none of the ports have it*
CP/M is pure ASCII with no function/navigation keys; each terminal defines its own
escape bytes. z80cpmw lets the user bind every special key (Up/Down/Left/Right,
Home/End, Insert/Delete, PageUp/PageDown, F1–F12) to arbitrary byte sequences
written as termcap-style escape strings.
- **Behaviour/spec:** escape syntax `\E` (=0x1B), `\n\r\t\b\f\s`, `\NNN` (octal),
  `^X` (control), `^?` (DEL); names case-insensitive (Ins/Del/PgUp/PgDn accepted);
  empty value unbinds. Defaults are VT220/xterm. `f1ToCpm` / `f5ToCpm` flags release
  the app's F1=Help / F5=Start-Stop shortcuts so those keys reach CP/M instead.
  **The full, copy-pasteable spec is `docs/CONFIGURATION.md`** (and the in-app
  Configuration help).
- **Where:** `z80cpmw/Keymap.h`, `TerminalView.cpp` (`setKeyBindings`,
  `handleKeyDown`), `Config.h` (`KeyboardConfig`: `f1ToCpm`,`f5ToCpm`,`keys`).
- **Config:** `keyboard` block in the JSON config.
- **Platform mapping:** the *byte sequences sent to CP/M* are the parity target.
  Mobile maps soft-key/hardware-key events; the CLI maps host-terminal keys. Adopt
  the same config schema (named keys → termcap strings) so configs are portable.

### 2. Scrollback history  — *new in Windows; verified absent in all three ports*
Lines that scroll off the top are retained so the user can scroll back (great for
long `DIR` listings).
- **Behaviour/spec:** ring buffer of full 80-col lines captured at the single
  scroll-off choke point; **mouse wheel** (3 lines/notch) and **Shift+PageUp/PageDown**
  (one page) scroll; **Ctrl+Home/End** jump to oldest/live; any key sent to CP/M or
  a paste snaps back to live; cursor hidden while viewing history; the view stays
  anchored as new output arrives; history cleared on emulator start/reset; **plain**
  PageUp/PageDown still go to CP/M. Capacity configurable; 0 disables.
- **Where:** `z80cpmw/TerminalView.{h,cpp}` (`m_scrollback`, `scrollUp` capture hook,
  `visibleCell`, `scrollByLines`, `WM_MOUSEWHEEL`, `handleKeyDown`).
- **Config:** `display.scrollbackLines` (default **1000**) + a Settings field.
- **Platform mapping:** GUI ports (iOS/macOS/Android) should implement an in-app
  buffer like this. The **Linux CLI can rely on the host terminal's scrollback** —
  document that rather than reimplementing.

### 3. Mouse text selection + Copy/Paste
Drag to select terminal text, right-click for Copy/Paste.
- **Behaviour/spec:** drag-select with inverted highlight; context-menu Copy/Paste;
  trailing-space trim; non-ASCII filtered; CRLF normalised to CR for CP/M on paste;
  Ctrl+C/Ctrl+V left untouched so they reach CP/M as ^C/^V; paste gated on emulator
  running.
- **Where:** `z80cpmw/TerminalView.cpp` (`handleLButtonDown/MouseMove/LButtonUp`,
  `showContextMenu`, `copySelectionToClipboard`, `pasteFromClipboard`).
- **Platform mapping:** macOS = native selection + ⌘C/⌘V; Android = the "control
  strip" Copy/Paste (cpmdroid already has this); Linux CLI = host terminal.

### 4. R8 / W8 host file transfer with arbitrary host paths
`R8 name` imports a host file into CP/M; `W8 name` exports.
- **Behaviour/spec:** **absolute / UNC / rooted paths are used verbatim**
  (e.g. `R8 C:\Users\me\Desktop\getkey2.com`); bare names resolve to the app data
  folder. On packaged builds the data folder's redirected real path is resolved and
  surfaced to the user.
- **Where:** `z80cpmw/emu_io_windows.cpp` (`resolveHostPath`,
  `emu_host_file_close_write`, `emu_io_get_data_folder_display`).
- **Platform mapping:** cpmdroid currently uses fixed `Imports/`/`Exports/` folders;
  parity = let the user reach **arbitrary** host locations within the platform's
  file model (document picker / Files app / real paths). Verify ioscpm's behaviour.

### 5. Remote disk catalog + downloader (pinned)
Download prebuilt disk images from the shared release host instead of bundling
copyrighted content.
- **Behaviour/spec:** fetch `disks.xml`, list catalog (name/desc/status), download
  with progress + cancel, track downloaded state, delete. **Pinned to one explicit
  release tag** (not `latest`) so a new release can't silently swap disk images out
  from under an installed client and re-introduce an HBIOS/CBIOS version mismatch.
- **Where:** `z80cpmw/DiskCatalog.{h,cpp}` — note the single `RELEASE_TAG` constant.
- **Shared concern:** all ports download from `ioscpm` releases. **Every port should
  pin to an explicit tag matching the RomWBW version its embedded ROM was built
  from.** See this repo's `WIP`/parity notes on the version-skew problem.

### 6. Remote help system + bundled fallback
In-app help fetched from GitHub, with offline bundled topics.
- **Behaviour/spec:** `help_index.json` + markdown topics downloaded and cached;
  bundled "Getting Started" and "Configuration" topics always available offline; a
  small markdown→text renderer (headers, tables, lists, inline code).
- **Where:** `z80cpmw/HelpWindow.{h,cpp}`. (ioscpm and cpmdroid already have help —
  align the topic set and the local fallback.)

### 7. NVRAM / autoboot / boot string
- **Behaviour/spec:** RomWBW autoboot config via `W` at the boot menu persists;
  "Clear Boot Config" resets it; an optional `bootString` is auto-typed at the boot
  menu. **Note the boot-unit numbering:** with the EMU AVW ROM the on-board RAM/ROM
  disks are units 0 and 1, so the first hard disk is unit **2** — see this repo's
  Getting Started help for the user-facing wording.
- **Where:** `z80cpmw/EmulatorEngine*.cpp` (`clearNvramSetting`, `setBootString`),
  config `core.bootString`. (ioscpm already has NVRAM boot config.)

### 8. Desktop window state (Windows/macOS only)
- **Behaviour/spec:** remember main-window position/size across runs with
  monitor-change / off-screen reset; auto-size the window to the exact 80×25 grid on
  font change; per-monitor DPI-v2 font scaling.
- **Where:** `z80cpmw/MainWindow.cpp` (`WindowConfig`, `resizeWindowToTerminal`),
  `TerminalView::createFont`; config `window` block. **N/A to mobile.**

### 9. Configurable font size
- **Where:** config `display.fontSize`, View menu (`MainWindow::onViewFontSize`).
  All GUI ports should expose this; mobile typically pinch-to-zoom.

### 10. Cromemco Dazzler graphics card (optional)
Emulated retro graphics card in a separate window.
- **Behaviour/spec:** enable + base I/O port + scale, rendered in its own window.
- **Where:** `z80cpmw/Dazzler.cpp`, `DazzlerWindow.cpp`; config `hardware.dazzler`.
- **Status:** appears partially present in the core/Android, absent in iOS. Low
  priority unless a port specifically wants it.

### 11. Config profiles & JSON config
- **Behaviour/spec:** named config profiles (save/load/delete); single JSON config
  file with migration from the legacy INI format.
- **Where:** `z80cpmw/Config.{h,cpp}` (`ConfigManager`). Each port keeps its own
  config format; parity is the *set of settings*, not the file format.

### 12. Manifest-disk write warning
- **Behaviour/spec:** warn before writing to a downloaded catalog ("manifest") disk,
  since a re-download would overwrite local changes. Suppressible.
- **Where:** config `core.warnManifestWrites`; `SettingsDialogWx.cpp`,
  `EmulatorEngine` disk-warning hooks.

---

## Per-port gap snapshot (verify before acting)

✅ present · ⬜ missing · ➖ N/A or host-provided · ❓ verify

| Feature | iOS/macOS `ioscpm` | Android `cpmdroid` | Linux/Web `romwbw_emu` |
| --- | :---: | :---: | :---: |
| 1. Configurable keymap (termcap) | ⬜ | ⬜ | ➖ (host terminal / browser) |
| 2. Scrollback | ⬜ | ⬜ | ➖ CLI (host terminal) · ✅ web (xterm.js) |
| 3. Mouse/native Copy-Paste | ✅ | ✅ (control strip) | ➖ |
| 4. R8/W8 arbitrary host paths | ❓ | ⬜ (fixed folders) | ✅ CLI (host paths) · ✅ web (picker/download) |
| 5. Disk catalog + **pinned** tag | ✅ / ❓ pinned | ✅ / ❓ pinned | ❓ CLI · ➖ web (same-origin server list) |
| 6. Help system + offline fallback | ✅ | ✅ | ⬜ |
| 7. NVRAM autoboot / bootString | ✅ | ❓ | ✅ (boot menu) |
| 8. Window state / DPI | ❓ (Mac) | ➖ | ➖ |
| 9. Font size setting | ❓ | ❓ | ➖ |
| 10. Dazzler | ⬜ | ❓ (partial) | ✅ (partial) |
| 11. Config profiles | ⬜ | ⬜ | ✅ CLI (JSON settings file, v1.34) · web persists UI selections (localStorage) |
| 12. Manifest write warning | ❓ | ❓ | ✅ web (with per-disk suppression) · ➖ CLI |

## Suggested priority order for each GUI port

1. **Configurable keymap (#1)** — biggest UX win, fully specced in
   `docs/CONFIGURATION.md`, and portable as a config schema.
2. **Scrollback (#2)** — small, self-contained, high user value; spec in
   `TerminalView.cpp`.
3. **Arbitrary-path R8/W8 (#4)** within the platform's file model.
4. **Pin the disk catalog to an explicit release tag (#5)** to stop HBIOS/CBIOS
   version drift.
5. Align **help topics / offline fallback (#6)** and **NVRAM/autoboot (#7)**.
6. Desktop-only: **window state + DPI (#8)** for the Mac build.
7. Optional: **profiles (#11)**, **Dazzler (#10)**.

For the Linux CLI, items 1/2/3/8 are mostly the host terminal's job; focus on the
catalog downloader (#5) and help (#6). The web/WASM frontend already covers
scrollback (#2), R8/W8 via browser picker/download (#4), same-origin disk
selection (#5, different model), UI-selection persistence (#11, lighter than
profiles), the manifest write warning (#12), and adds a dirty-disk warning
before tab close (v1.34).
