# Feature Parity Guide — z80cpmw (Windows) → sibling ports

**Purpose.** The Windows front-end (`z80cpmw`) has the richest *app* UX of the
RomWBW/CP/M emulator family. This document is the canonical checklist of those
features so the other ports can reach parity. Point a coding agent in a sibling
repo at this file (raw URL works) and have it implement the gaps against the
referenced z80cpmw source.

**One item now runs the other way.** Terminal emulation (item 13) is no longer a
Windows strength: `ioscpm` and `cpmdroid` both implement VT52 plus a much fuller
VT100 than this repo does. For that item the reference source is theirs, not ours.

Last reviewed: **2026-08-07**. On that date the **Android (`cpmdroid`) column was
re-verified against that port's source**, at commit `c26aeb7` (version 1.19 /
versionCode 20, branch `v1.19-parity-sync`) — key map (`TerminalView.kt`:
`DEFAULT_KEY_BINDINGS`, `decodeKeySequence`, `handleKeyDown`), scrollback
(`TerminalView.kt`: `history`, `scrollRegionUp`, `onDraw`, `resetTerminal`),
R8/W8 (`MainActivity.kt`: `showFileTransferDialog`, `importPickedFile`,
`shareExportedFile`, `res/xml/file_paths.xml`), help fallback (`HelpActivity.kt`,
`HelpTopicActivity.kt`, `app/build.gradle.kts`), catalog pinning
(`data/DiskCatalogRepository.kt`), NVRAM (`data/SettingsRepository.kt`,
`MainActivity.kt`), font size (`SettingsActivity.kt`,
`data/SettingsRepository.kt`), the manifest write warning, and the Dazzler stubs
(`app/src/main/cpp/emu_io_android.cpp`). Two iOS/macOS rows were corrected from
`ioscpm` source at the same time (item 1 key map, item 5 catalog pinning), and
the item 13 statuses come from `ioscpm/iOSCPM/Views/EmulatorViewModel.swift`,
`z80cpmw/TerminalView.cpp` and `romwbw_emu/src/emu_io_cli.cc` +
`romwbw_emu/web/romwbw.html-template`. Everything else in the `ioscpm` and
`romwbw_emu` columns is an older best-effort reading; item 4's `ioscpm`
paragraph is still as verified on **2026-07-23**.

The Android column describes commit `c26aeb7` plus the follow-up commit that
finished the release: an on-screen key row driven by the key map, configuration
profiles, a scrolled-back indicator, catalog-version tracking, and crash
diagnostics. Rows 1 and 11 were re-checked against that later state.

Port-status columns are best-effort — **verify against the port's current
source** before acting (local checkouts may lag the ports' latest builds);
absence of a mention is not proof of absence.

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

### 1. Configurable keyboard map (termcap-style)  — *done on Android (cpmdroid 1.19); partial on iOS/macOS*
CP/M is pure ASCII with no function/navigation keys; each terminal defines its own
escape bytes. z80cpmw lets the user bind every special key (Up/Down/Left/Right,
Home/End, Insert/Delete, PageUp/PageDown, F1–F12) to arbitrary byte sequences
written as termcap-style escape strings.
- **Behaviour/spec:** escape syntax `\E` (=0x1B), `\n\r\t\b\f\s`, `\NNN` (octal),
  `^X` (control), `^?` (DEL); names case-insensitive (Ins/Del/PgUp/PgDn accepted);
  empty value unbinds. Defaults are VT220/xterm. `f1ToCpm` / `f5ToCpm` /
  `ctrlRToCpm` flags decide who receives the keys that double as app shortcuts
  (F1=Help, F5/Shift+F5=Start/Stop, Ctrl+R=Reset); a key claimed by the app is
  swallowed whole and CP/M never sees it. F1/F5 default to the app because CP/M
  has no function keys; `ctrlRToCpm` defaults to **true** because `^R` (0x12) is
  ASCII CP/M really reads, so Reset has no default shortcut and lives on the
  Emulator menu.
  **The full, copy-pasteable spec is `docs/CONFIGURATION.md`** (and the in-app
  Configuration help).
- **Where:** `z80cpmw/Keymap.h`, `TerminalView.cpp` (`setKeyBindings`,
  `handleKeyDown`), `Config.h` (`KeyboardConfig`: `f1ToCpm`,`f5ToCpm`,
  `ctrlRToCpm`,`keys`), `MainWindow.cpp` (`rebuildAccelerators`,
  `updateMenuAccelHints`).
- **Config:** `keyboard` block in the JSON config.
- **Platform mapping:** the *byte sequences sent to CP/M* are the parity target.
  Mobile maps soft-key/hardware-key events; the CLI maps host-terminal keys. Adopt
  the same config schema (named keys → termcap strings) so configs are portable.
- **Verified port behaviour (2026-08-07):**
  - **cpmdroid (Android)** — has it, and **a map really is portable to and from
    z80cpmw**. `TerminalView.DEFAULT_KEY_BINDINGS` holds the same 22 names as
    `Keymap.h`'s `defaultBindings()` with the same termcap strings — the two
    tables were extracted and compared mechanically: identical name set,
    identical values (`Up`=`\E[A` … `Delete`=`^?` … `F12`=`\E[24~`).
    `decodeKeySequence` accepts the same syntax as `keymap::decode` (`\E`/`\e`,
    `\n \r \t \b \f \s`, `\\`, `\^`, 1–3 octal digits after `\`, `^X` as
    `toupper(X) & 0x1F`, `^?` = 0x7F, anything else literal), so the strings
    decode to the same bytes on both.
    **Two caveats before copying a map across.** (a) *Names must be canonical.*
    `Keymap.h::vkForName` lower-cases the name and accepts aliases
    (`ins`, `del`, `pgup`, `pgdn`, `prior`, `next`); Android looks the name up
    verbatim in `DEFAULT_KEY_BINDINGS`, so `pgup` or `up` is not recognised and
    that key silently keeps its default. Use `Up`…`PageDown`, `F1`…`F12`.
    (b) *The file is not portable, only the values.* Windows keeps them in
    `z80cpmw.json` under `keyboard.keys` (backslashes doubled for JSON);
    Android keeps one SharedPreference per edited key
    (`keymap_<Name>`, `SettingsRepository.saveKeyBindings`) and stores **only
    keys the user changed**, so a later change to the defaults still reaches
    everyone else. An empty value unbinds on both (Android swallows the key
    rather than falling through to the printable path).
    There is no `f1ToCpm`/`f5ToCpm`/`ctrlRToCpm` equivalent and none is needed:
    F1/F5 are not app shortcuts on Android, so all twelve F-keys always reach
    CP/M, and Reset has no Ctrl+R accelerator to compete with `^R`.
    A hardware keyboard reaches the map through `bindingNameFor`
    (`KEYCODE_DPAD_*`, `MOVE_HOME`, `MOVE_END`, `INSERT`, `FORWARD_DEL`,
    `PAGE_UP`, `PAGE_DOWN`, `F1`–`F12`); a touch-only device reaches it through
    an **on-screen key row** toggled from the toolbar (arrows, Home/End,
    PageUp/PageDown, Insert/Delete, F1–F12). Those buttons hold no sequences of
    their own — each calls `TerminalView.sendNamedKey(name)`, so a remapping
    applies to them too. This is the piece the other GUI ports still lack: on
    iOS the map is likewise only reachable from a hardware keyboard. Editor:
    Settings → **Keyboard Map** (one field per key, **Defaults** button restores
    the VT220 table).
  - **ioscpm (iOS/macOS)** — ◐. `iOSCPM/Views/TerminalView.swift` has the same
    termcap escape schema (`KeyMap.expand`; it has no explicit `\^` case but its
    default arm emits the same literal `^`, so every documented escape decodes to
    the same bytes) and a per-key editor. What differs is the *shape* of the map:
    the key set is **ten keys** (arrows, Home/End, PageUp/PageDown,
    Insert/Delete) with **no F1–F12**; the names are lower-camel raw values
    (`up`, `pageUp`) rather than `Up`/`PageUp`; and it is organised as named
    profiles (`WordStar`, `VT100/ANSI`, `VT52`, `Custom`) whose default is the
    **WordStar diamond** (`^E`/`^X`/`^S`/`^D`), not the VT220 table — and even
    its `VT100/ANSI` profile binds Delete to `\E[3~` where z80cpmw and cpmdroid
    send `^?`. Individual values copy across; a whole map does not.

### 2. Scrollback history  — *new in Windows; done on iOS/macOS (ioscpm build 43) and Android (cpmdroid 1.19)*
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
- **Verified Android behaviour (2026-08-07), point by point against the spec above:**
  - **Matches.** Capacity is a setting — `SettingsRepository.DEFAULT_SCROLLBACK_LINES`
    = **1000**, `0` disables, capped at 10000, edited in Settings
    (`scrollbackLinesEdit`) and pushed in by `MainActivity.applyTerminalSettings`.
    Capture happens at a single scroll-off choke point (`scrollRegionUp`), and
    deliberately **only when the scrolling region starts at row 0**, so a status-line
    layout does not push its region lines into the history. Snap-to-live on any
    key sent to CP/M and on paste (`sendChar`/`sendBytes` → `snapToLive`;
    `pasteFromClipboard` goes through `sendChar`), while terminal answerbacks use
    a separate `replyListener` so they do **not** yank the user out of history.
    The view stays anchored as new output arrives (`userScrollUp++`, with the
    in-flight drag origin moved too). The cursor is hidden while reading history
    (`onDraw` draws it only when `scroll == 0`). **Shift+PageUp/PageDown** move
    one page and **Ctrl+Home/End** jump to oldest/live, exactly the Windows
    chords; **plain PageUp/PageDown still go to CP/M** through the key map.
    History is cleared on a cold boot (`MainActivity.bootEmulation` →
    `resetTerminal`, which also resets VT52 mode, the scrolling region and any
    half-parsed escape).
  - **Does not match.** No mouse wheel — the touch equivalent is **drag**
    (`onTouchEvent`; drag down reveals older lines), so the "3 lines/notch" part
    has no counterpart. Copy takes history *and* screen but is capped at 4000
    lines / 200000 characters, because the clipboard crosses a Binder
    transaction that a full 10000-line buffer would overflow. And after a
    full-screen erase cpmdroid stops **drawing** history above the live rows
    until output scrolls from the top again (`historyIsContiguous`,
    `visibleHistorySize`) — the lines are kept, not dropped, but they are hidden;
    z80cpmw has no such rule. The page/jump chords also need a hardware
    keyboard, as item 1 notes.

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

### 4. R8 / W8 host file transfer — arbitrary host paths + a findable data folder
`R8 name` imports a host file into CP/M; `W8 name` exports. **User-facing doc:
[`docs/FILE_TRANSFER.md`](docs/FILE_TRANSFER.md)** covers where files land and how
to find them on every platform.
- **Behaviour/spec (Windows):** **absolute / UNC / rooted paths are used verbatim**
  (e.g. `R8 C:\Users\me\Desktop\getkey2.com`), even on the full-trust Store build;
  bare names resolve to the app data folder. The data folder's redirected real path
  is resolved (`GetFinalPathNameByHandle`) and surfaced in **About**, **Settings**
  (copyable + Open Folder), and the **boot banner**.
- **Where:** `z80cpmw/emu_io_windows.cpp` (`resolveHostPath`, `isAbsolutePath`,
  `emu_host_file_close_write`, `emu_io_get_data_folder_display` / `resolveRealPath`).
- **Verified port behaviour:**
  - **ioscpm (iOS/macOS)** *(2026-07-23)* — `W8` always writes `Documents/Exports`, `R8` always
    reads `Documents/Imports` (no per-transfer dialog). As of **v1.4.11 / build 41**
    an **Import File…** picker (enabled on iOS *and* Mac Catalyst) stages an
    arbitrary-location file into `Imports` for a later `R8`; the old opt-in
    per-transfer picker was removed. So arbitrary-path *import* is covered (via
    staging), but `W8` export still has no save-as/arbitrary path. Findability is
    good: iOS exposes Documents to the **Files app** (`UIFileSharingEnabled` +
    `LSSupportsOpeningDocumentsInPlace`), and both platforms have **Open
    Imports/Exports Folder** menu items.
  - **cpmdroid (Android)** *(2026-08-07, v1.19 / `c26aeb7` — this supersedes the
    2026-07-23 reading, which described a fixed-folder arrangement with no UI)* —
    the commands themselves are unchanged and still take **no host path**: `R8`
    reads only `Imports/` and `W8` writes only `Exports/`, both under
    `getExternalFilesDir(null)` (`/Android/data/com.awohl.cpmdroid/files/…`);
    `R8` rejects a name containing `/`, `\` or `..`, and `W8` strips any path
    components off the guest-supplied name, so neither can escape. What changed
    is everything around them, and it closes the findability hole:
    a **Files** button in the control strip opens a dialog that prints both
    absolute paths with a **Copy Paths** action; **Import File…** launches a SAF
    picker (`ActivityResultContracts.OpenDocument`, `*/*`) that stages a file
    from anywhere on the device into `Imports/` under a sanitised 8.3 name
    (numbered instead of overwriting when two sources collide), ready for a later
    `R8`; and each export can leave the sandbox by **Share Export…** in that
    dialog or the **SHARE** action on the snackbar `W8` raises, both going
    through `ACTION_SEND` on a `FileProvider` URI. `res/xml/file_paths.xml`
    exposes `Exports/` **only** — disk images and preferences stay private.
    Net effect: Android now sits exactly where iOS does — arbitrary-location
    *import* by staging, **no arbitrary-path export** — and the Android 11+
    "`Android/data` is invisible in the Files app" trap no longer strands a file.
- **Parity targets:** (a) let users reach **arbitrary** host locations within each
  platform's file model — a document picker / `ACTION_CREATE_DOCUMENT`; and (b) at
  minimum, **make exports findable**. (b) is now **done on Android** (share sheet
  plus the paths shown in-app); (a) is still open on both mobile ports, which
  need a save-as for `W8` — `ACTION_CREATE_DOCUMENT` on Android, a document
  exporter on iOS — before either can be called ✅. The shared iOS/Mac
  **`help_file_transfer.md`** was stale (wrong bundle id `com.awohl.iOSCPM`,
  wrong app name "iOSCPM", no mention of Import File…); `ioscpm` commit `9a9d7fd`
  fixed it on 2026-07-23. cpmdroid ships its own Android-worded
  `help/help_file_transfer.md` as of 1.19, so the two no longer share that text —
  a change to one no longer fixes the other.

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
- **Verified port behaviour (2026-08-07):**
  - **cpmdroid (Android)** — **pinned**. `data/DiskCatalogRepository.kt` builds
    both the catalog URL and the download base from a single
    `RELEASE_TAG = "v1.4.5"`, with the reason recorded in a comment (the core's
    HBIOS reports RomWBW v3.5.1, and slices from other releases print an
    HBIOS/CBIOS mismatch). Help deliberately stays on `releases/latest` — see
    item 6 for why that choice is only safe with a bundled fallback.
  - **ioscpm (iOS/macOS)** — **not pinned**. `EmulatorViewModel.swift` uses
    `releases/latest/download` for both `disks.xml` and the images, which is
    precisely the silent-swap case this item exists to prevent. (`ioscpm`
    `ac586b5` files it as a task, so it is known there.)

### 6. Remote help system + bundled fallback
In-app help fetched from GitHub, with offline bundled topics.
- **Behaviour/spec:** `help_index.json` + markdown topics downloaded and cached;
  bundled "Getting Started" and "Configuration" topics always available offline; a
  small markdown→text renderer (headers, tables, lists, inline code).
- **Where:** `z80cpmw/HelpWindow.{h,cpp}`. (ioscpm and cpmdroid already have help —
  align the topic set and the local fallback.)
- **Why the fallback is not optional — a trap every port shares.** cpmdroid
  fetched its index from `releases/latest/download/help_index.json` with **no**
  bundled copy. GitHub serves that URL from whatever the newest release is, and
  the help assets stopped being attached after v1.11, so from that release on
  **every shipped build got a 404 and had no help at all** — and nothing in the
  app or the release process failed, because a missing help topic looks exactly
  like an offline user. Any port that resolves help (or any other content)
  through `releases/latest` is one un-attached asset away from the same silent
  break. Ship the topics in the app and treat the download as an optional
  refresh, never as the source.
- **Verified port behaviour (2026-08-07):** **cpmdroid** fixed this in 1.19.
  `app/build.gradle.kts` adds the repo's `help/` directory as an assets source
  dir, so `help_index.json` and all seven topics (quick start, CP/M 2.2, file
  transfer, NZCOM, QPM, ZPM3, ZSDOS) ship inside the APK.
  `HelpActivity.loadHelpIndex` still **prefers** the published index — so a
  correction can go out without an app update — but falls back to the bundled
  one, and `HelpTopicActivity` falls back per topic to the bundled file of the
  same name. Help now works offline and survives an unpublished release asset.

### 7. NVRAM / autoboot / boot string
- **Behaviour/spec:** RomWBW autoboot config via `W` at the boot menu persists;
  "Clear Boot Config" resets it; an optional `bootString` is auto-typed at the boot
  menu. **Note the boot-unit numbering:** with the EMU AVW ROM the on-board RAM/ROM
  disks are units 0 and 1, so the first hard disk is unit **2** — see this repo's
  Getting Started help for the user-facing wording.
- **Where:** `z80cpmw/EmulatorEngine*.cpp` (`clearNvramSetting`, `setBootString`),
  config `core.bootString`. (ioscpm already has NVRAM boot config.)
- **Verified Android behaviour (2026-08-07):** NVRAM persistence is **present**.
  `EmulatorEngine` exposes the string-based native API
  (`setNvramSetting`/`getNvramSetting`/`hasNvramChange`/`isNvramInitialized`);
  `MainActivity` restores the saved setting right after `completeInit`, saves it
  about every 5 s when the dirty flag is set, saves again on pause, and also
  compares against the stored value to catch `SYSCONF` edits that do not raise
  the flag; `SettingsRepository` keeps it under the `nvram` preference; Settings
  has **Clear Boot Config**. The **`bootString` auto-type has no counterpart** —
  nothing in the Android source types a string at the boot menu (the only
  automatic input is a single CR sent 500 ms after load to make the ROM print
  its prompt).

### 8. Desktop window state (Windows/macOS only)
- **Behaviour/spec:** remember main-window position/size across runs with
  monitor-change / off-screen reset; auto-size the window to the exact 80×25 grid on
  font change; per-monitor DPI-v2 font scaling.
- **Where:** `z80cpmw/MainWindow.cpp` (`WindowConfig`, `resizeWindowToTerminal`),
  `TerminalView::createFont`; config `window` block. **N/A to mobile.**

### 9. Configurable font size
- **Where:** config `display.fontSize`, View menu (`MainWindow::onViewFontSize`).
  All GUI ports should expose this; mobile typically pinch-to-zoom.
- **Verified Android behaviour (2026-08-07):** **present** — a slider in Settings
  (`fontSizeSeekBar`, shown in pt) stored as the `font_size` preference, applied
  through `TerminalView.customFontSize`. There is **no** pinch-to-zoom.
  Range is clamped **in the repository**, 8–24, not only on the slider: `android:min`
  on a `SeekBar` needs API 26 and is ignored on 24–25, where the slider reaches 0 —
  and a zero font size saturates the column arithmetic to `Int.MAX_VALUE` and kills
  the app on every launch once the value has been persisted. Worth copying: clamp
  where the value is *stored*, not where it is *entered*.

### 10. Cromemco Dazzler graphics card (optional)
Emulated retro graphics card in a separate window.
- **Behaviour/spec:** enable + base I/O port + scale, rendered in its own window.
- **Where:** `z80cpmw/Dazzler.cpp`, `DazzlerWindow.cpp`; config `hardware.dazzler`.
- **Status:** absent in iOS. **Android is not partial — it is stubbed out on
  purpose** (verified 2026-08-07): `app/src/main/cpp/emu_io_android.cpp` defines
  `dazzler_port_in` returning 0 and `dazzler_port_out` doing nothing, both marked
  "stubs - not used on Android", next to the same treatment of the DSKY. Nothing
  in the Kotlin layer mentions the Dazzler, and there is no window, no setting
  and no rendering. Treat Android as ⬜, not ◐: a guest can write to the ports
  without an error, and nothing whatever happens. Low priority unless a port
  specifically wants it.

### 11. Config profiles & JSON config
- **Behaviour/spec:** named config profiles (save/load/delete); single JSON config
  file with migration from the legacy INI format.
- **Where:** `z80cpmw/Config.{h,cpp}` (`ConfigManager`). Each port keeps its own
  config format; parity is the *set of settings*, not the file format.
- **cpmdroid (Android)** — ✅. `SettingsRepository.{saveProfile,loadProfile,
  listProfiles,deleteProfile}` store a profile as JSON inside the existing
  preferences (ROM, four disk slots, font size, wrap, scrollback capacity,
  sound, manifest-warning, NVRAM boot setting and the key-map overrides). UI in
  Settings → **Configuration Profiles**. Loading replaces the key map wholesale
  rather than merging it, so a profile that omits a key means that key is at its
  default. Disk *images* are never touched by any profile operation.

### 12. Manifest-disk write warning
- **Behaviour/spec:** warn before writing to a downloaded catalog ("manifest") disk,
  since a re-download would overwrite local changes. Suppressible.
- **Where:** config `core.warnManifestWrites`; `SettingsDialogWx.cpp`,
  `EmulatorEngine` disk-warning hooks.
- **Verified Android behaviour (2026-08-07):** **present and suppressible.**
  Downloaded (catalog) disks are flagged per unit through
  `EmulatorEngine.setDiskIsManifest` as they are mounted; the emulation loop
  polls `checkManifestWriteWarning` and raises a dialog **once per session**;
  Settings has a *Warn on manifest writes* checkbox
  (`SettingsRepository.isWarnManifestWritesEnabled`, default **true**), and the
  preference migration deliberately drops the pre-v3 stored `false` so the newer
  default takes effect. One asymmetry, if you copy the code: turning the
  checkbox **off** pushes `setDiskWarningSuppressed(unit, true)` to all 16 units
  (`applyManifestWarningPreference`), but turning it back **on** does not clear
  the suppression — that waits for the next disk reload or boot.

### 13. Terminal emulation (VT100/ANSI + VT52)
The front end **is** the terminal, so its escape-sequence coverage decides which
CP/M software actually runs: WordStar, Zork, TERMDEF, VDE and anything else
full-screen. This belongs in a front-end parity catalog for the same reason
copy/paste does. This repo was **not** the reference for it - `ioscpm` wrote the
parser and `cpmdroid` extended it - but it has now caught up.
- **Behaviour/spec (what the two mobile ports implement, and the target):** VT100/ANSI
  by default with **VT52** auto-detected from any VT52-exclusive sequence and left
  by `ESC <` / `ESC [ ? 2 h`; `ESC 7`/`8` (DECSC/DECRC), `ESC D`/`M`/`E`
  (IND/RI/NEL); CSI `A B C D`, `G`/`` ` ``, `d`, `H`/`f`, `J`, `K`, `L`, `M`,
  `P`, `@`, `X`, `S`, `T`, `m`, `s`, `u`, `r` (**DECSTBM** scrolling region),
  `n` (DSR/CPR answerback), `c` (DA answerback), `h`/`l` (DECANM, DECAWM,
  DECTCEM); **deferred autowrap** (writing the last column arms the wrap instead
  of scrolling immediately); character-set designators (`ESC ( ) * + #` and
  `ESC SP`) consumed with their parameter byte rather than leaking as glyphs;
  per-cell foreground **and background** so reverse video renders; TAB advancing
  to the next 8-column stop.
- **Where (per port):**
  - **ioscpm** — `iOSCPM/Views/EmulatorViewModel.swift`. The origin of the
    parser: full VT52, scrolling region, answerbacks, deferred autowrap, charset
    consumption. Missing only `P` (DCH), `@` (ICH), `X` (ECH), `S`/`T` (SU/SD),
    and its DEC private modes other than DECANM are acknowledged but not acted on
    (so DECAWM and DECTCEM do nothing).
  - **cpmdroid** — `app/src/main/java/com/awohl/cpmdroid/TerminalView.kt`, ported
    from the above in 1.19 and slightly ahead of it: it adds `P @ X S T` and acts
    on DECAWM (7) and DECTCEM (25). Parser input is bounded (16 params, 6 digits,
    2 intermediates) against a runaway guest.
  - **z80cpmw (this repo)** — `TerminalView.cpp`
    (`processEscapeChar`, `processCSIChar`, `executeCSI`, `applySGR`).
    **VT52 and the answerbacks landed**, ported from `cpmdroid`: the full VT52
    set (`ESC A B C D E F G H I J K Y Z <`, with `D`/`E`/`H` overloaded by mode),
    auto-detection from any VT52-exclusive sequence, `ESC <` and DECANM
    (`ESC[?2h`/`l`) to switch, and `n`/`c`/`ESC Z` answerback so a program that
    asks the terminal to identify itself or report the cursor no longer waits
    forever. The parser also learned **private parameters**: `?` (and `<`, `=`,
    `>`) used to be treated as a *final* byte, so `ESC [ ? 2 5 l` ended at the
    `?` and printed `25l` as text — which meant nothing beginning `ESC[?` could
    ever work, DECANM included. DECTCEM (`?25`) now hides the cursor for real,
    through a flag separate from the blink phase. Covered by
    `test_vt52.cpp` (27 checks, headless, built by `compile_test_vt52.cmd`).

    The rest followed: CSI `G`/`` ` ``/`d`, `L M P @ X S T`, and `r`
    (**DECSTBM**), with LF, IND and RI honouring the region and a partial region
    deliberately *not* feeding the scrollback — lines pushed out of a status-line
    window were never history. **Deferred autowrap** and DECAWM, so writing the
    bottom-right cell no longer scrolls the screen. Character-set and line-size
    designators are consumed with their parameter byte (`ESC ( B` used to print
    its `B`). SGR gained `22`, and `27` now undoes the reverse-video swap
    instead of resetting the whole attribute byte — a colour set while reversed
    is applied in the un-swapped domain so it lands in the nibble that shows.
    LF implies CR, matching both mobile ports, so an LF-only text file no longer
    stair-steps.

    Covered by `test_vt52.cpp`: **73 checks**, headless, built and run by
    `compile_test_vt52.cmd`. It drives the terminal through the public interface
    only — cursor state is read back with `ESC [ 6 n`, which puts the answerback
    under test rather than assuming it, and screen content through `cellAt()`.

    **Still missing here:** the parser input is unbounded where `cpmdroid`'s
    caps params, digits and intermediates; there is no per-cell attribute
    beyond the packed CGA byte; and `clear()` still resets the attribute and
    escape state, which `ESC [ 2 J` alone should not do.
  - **romwbw_emu** — CLI writes bytes straight to the host terminal
    (`emu_io_cli.cc`, `putchar`), so the host terminal is the emulation; the web
    build uses **xterm.js 5.3** (`web/romwbw.html-template`), which is a far more
    complete VT than any of the native front ends.
- **Parity target:** the mobile ports' coverage, i.e. run WordStar and Zork
  without the screen breaking up. **The gap here is z80cpmw's**, and the fix is a
  port in the reverse direction, from `TerminalView.kt` / `EmulatorViewModel.swift`
  back into `TerminalView.cpp`.

---

## Per-port gap snapshot (verify before acting)

✅ present · ◐ partial · ⬜ missing · ➖ N/A or host-provided · ❓ verify

Android `cpmdroid` is as of **v1.19 / `c26aeb7` (2026-08-07, from source)**.

| Feature | iOS/macOS `ioscpm` | Android `cpmdroid` | Linux/Web `romwbw_emu` |
| --- | :---: | :---: | :---: |
| 1. Configurable keymap (termcap) | ◐ (10 keys, no F1–F12, WordStar default) | ✅ (same 22 names + syntax as `Keymap.h`; on-screen key row too) | ➖ (host terminal / browser) |
| 2. Scrollback | ✅ | ✅ (setting, drag instead of wheel) | ➖ CLI (host terminal) · ✅ web (xterm.js) |
| 3. Mouse/native Copy-Paste | ✅ | ✅ (control strip) | ➖ |
| 4. R8/W8 arbitrary host paths | ◐ (R8 via Import File…; W8 fixed) | ◐ (R8 via SAF import; W8 fixed folder + Share) | ✅ CLI (host paths) · ✅ web (picker/download) |
| 5. Disk catalog + **pinned** tag | ✅ / ⬜ not pinned (`releases/latest`) | ✅ / ✅ pinned (`v1.4.5`) | ❓ CLI · ➖ web (same-origin server list) |
| 6. Help system + offline fallback | ✅ | ✅ (bundled in APK since 1.19) | ⬜ |
| 7. NVRAM autoboot / bootString | ✅ | ✅ NVRAM / ⬜ bootString | ✅ (boot menu) |
| 8. Window state / DPI | ❓ (Mac) | ➖ | ➖ |
| 9. Font size setting | ❓ | ✅ (Settings slider, 8–24pt) | ➖ |
| 10. Dazzler | ⬜ | ⬜ (explicit no-op stubs) | ✅ (partial) |
| 11. Config profiles | ⬜ | ✅ (named; ROM, disks, boot, terminal, keymap) | ✅ CLI (JSON settings file, v1.34) · web persists UI selections (localStorage) |
| 12. Manifest write warning | ❓ | ✅ (suppressible, once per session) | ✅ web (with per-disk suppression) · ➖ CLI |
| 13. Terminal emulation (VT100 + VT52) | ✅ | ✅ (+ ICH/DCH/ECH/SU/SD, DECAWM, DECTCEM) | ➖ CLI (host terminal) · ✅ web (xterm.js) |

**z80cpmw's own row 13 is ◐** — see item 13 for what is missing here. Every other
row in this document is ✅ for z80cpmw by construction; that one is not.

## Suggested priority order for each GUI port

1. **Terminal emulation (#13)** — decides which software runs at all, so it
   outranks everything else. **All three ports now implement it**: z80cpmw
   caught up last, and the three parsers agree on VT52, the scrolling region,
   deferred autowrap, the editing finals and the answerbacks. What differs is
   detail, listed per port above.
2. **Configurable keymap (#1)** — biggest remaining UX win, fully specced in
   `docs/CONFIGURATION.md`, and portable as a config schema. For iOS/macOS the
   remaining work is the F1–F12 half, the canonical key names, and an on-screen
   way to press them — Android's key row (`buildKeyRow` in `MainActivity.kt`,
   backed by `TerminalView.sendNamedKey`) is the worked example.
3. **Scrollback (#2)** — small, self-contained, high user value; spec in
   `TerminalView.cpp`. Done on iOS/macOS and Android.
4. **Arbitrary-path R8/W8 (#4)** within the platform's file model — now only the
   export half (`ACTION_CREATE_DOCUMENT` on Android, a document exporter on iOS).
5. **Pin the disk catalog to an explicit release tag (#5)** to stop HBIOS/CBIOS
   version drift. Done on Android (`v1.4.5`); **still open on iOS/macOS**.
6. Align **help topics / offline fallback (#6)** and **NVRAM/autoboot (#7)** —
   for Android the remaining piece of #7 is the `bootString` auto-type.
7. Desktop-only: **window state + DPI (#8)** for the Mac build.
8. Optional: **profiles (#11)**, **Dazzler (#10)**.

For the Linux CLI, items 1/2/3/8/13 are mostly the host terminal's job; focus on the
catalog downloader (#5) and help (#6). The web/WASM frontend already covers
scrollback (#2), R8/W8 via browser picker/download (#4), same-origin disk
selection (#5, different model), UI-selection persistence (#11, lighter than
profiles), the manifest write warning (#12), terminal emulation (#13, xterm.js),
and adds a dirty-disk warning before tab close (v1.34).
