# Feature Parity Guide — z80cpmw (Windows) → sibling ports

**Purpose.** The Windows front-end (`z80cpmw`) has the richest *app* UX of the
RomWBW/CP/M emulator family. This document is the canonical checklist of those
features so the other ports can reach parity. Point a coding agent in a sibling
repo at this file (raw URL works) and have it implement the gaps against the
referenced z80cpmw source.

**Item 13 has been closed from this side.** Terminal emulation used to be the
one row where a mobile port led: `ioscpm` wrote the VT100/VT52 parser. (This
used to say `cpmdroid` extended it; it did not — see item 13 and the note
below.) z80cpmw ported that work back in **1.0.20**, which
reached users as the signed sideload beta **1.0.21-beta** and then, from a
single build, as Store **1.0.22** and sideload **1.0.22-beta** (both
2026-08-23). All three front ends now cover VT52, DECSTBM, deferred autowrap
and the answerbacks; what still differs is the per-port detail listed in item
13.

**The last of this repo's own residue in that row closed on 2026-08-28.** The
one thing item 13 still named — no per-cell attribute beyond the packed CGA
byte — landed in two commits, `480edcb` for the parser's flags byte and
`29d3438` for the multi-`HFONT` paint path, so row 13 is **✅** here now.
**It is not in a shipped build.** Store **1.0.22** and sideload
**1.0.22-beta** are the 1.0.20 parser; the attribute work, and the bright half
of the palette that landed beside it, are unreleased.

**The Android (`cpmdroid`) column was rewritten from source on 2026-08-25**, at
`origin/master` — every row, because the branch the previous review described
never existed. The 2026-08-07 review claimed to have read commit `c26aeb7`
(version 1.19, branch `v1.19-parity-sync`) and cited symbols from it throughout:
`DEFAULT_KEY_BINDINGS`, `decodeKeySequence`, `saveProfile`,
`showFileTransferDialog`, `res/xml/file_paths.xml`, an on-screen key row, VT52
and DECSTBM. **None of those objects or symbols exist in that repository**, and
`c26aeb7` is not a valid object in it. Five rows were overstated as a result and
are corrected below — 1, 4, 6, 11 and 13 — three of them from ✅ to ⬜. This was
a standing dispute in both repositories' `todo.txt`; it is settled by reading
the pushed branch, and the branch is not coming.

Some of that column moved on the same day for a better reason: `cpmdroid`
absorbed the v1.36 core and closed four gaps this document names — F1–F12,
the Ctrl window, terminal scrollback as a setting, and TAB. Those are marked
where they land.

**The iOS/macOS column was re-read from `ioscpm` source on 2026-08-26**, at
**build 52** (`49851aa`), and for the first time from a checkout on this machine
rather than at a distance. It supersedes the 2026-08-24 reading at build 50,
which was right when it was written and went stale within the day: build 51
(`4deea96`) landed that afternoon and build 52 (`bb5543f`) the next morning.

What the 2026-08-24 pass corrected stands. Item 5 said the disk catalog was
unpinned when it has been pinned to `v1.4.5` since build 42; item 6 claimed the
help system outright when the offline fallback that item exists for was absent;
items 8, 9 and 12 were unverified `❓` when window state is missing and font size
and the manifest warning are both present; item 13 gained the parser bounds
build 49 added.

**Build 51 closed the three complaints this document still had against that
port**, and each is corrected in the row below rather than only here. Item 1's
"ten keys, no F1–F12" is gone: `SpecialKey` in `iOSCPM/Views/KeyMap.swift` is
twenty-two cases and carries this repo's own VT220 byte table. Item 6's missing
offline fallback is gone: the index and all seven topics ship inside the app,
behind the download and the cache. Item 13's "missing only `P @ X S T`" is gone,
and DECAWM and DECTCEM are acted on rather than merely parsed.

**Build 52 is not a parity change**, and is the reason this column is worth
re-reading rather than re-dating: `W8 ANYFILE.TXT ..` destroyed the whole
`Documents` folder — `Disks`, `Imports` and `Exports`, so every disk image the
user had downloaded — and reported success to the guest. `R8` had the matching
bug on the read side, falling back to the *first file in the folder* when the
requested name missed. Both are fixed, in three overlapping layers; see item 4.
This is why refreshing the shared disk images is sequenced behind the port
fixes — `romwbw_emu` `docs/RELEASE_ORDER_2026-08-25.md`, of which that build is
step 1.

**The Linux/Web `romwbw_emu` column was swept from source on 2026-08-24** as
well — all thirteen rows, CLI and web frontend separately. It was the last
best-effort column and it was the least accurate: **eleven of thirteen rows
moved.** The corrections that matter most, because each was a ✅ that did not
survive reading the code:

- **10 Dazzler** was ✅ (partial) and there is no Dazzler code at all.
- **13 terminal (web)** was ✅ on the strength of xterm.js; the output filter
  never delivered TAB, BEL, FF or anything ≥ 0x7F to it. *(Fixed upstream in
  `2dbf6f2`, hours after this sweep was written; the row is ✅ again.)*
- **4 R8/W8 (CLI)** was ✅ "host paths"; only R8 took one, W8 wrote to CWD.
  *(Fixed upstream in `98eb6a1`, 50 minutes after this sweep was committed: W8
  now takes `<cpmname> [hostpath]`. The row is ✅ again.)*
- **5 catalog (CLI)** was ❓; there is no catalog feature, so it is N/A.

Rows 8 and 9 were the only two that stood. Three more were narrowed from ✅ to
◐ on evidence — 7 (web NVRAM is never read back), 11 (a settings file is not
named profiles), 12 (*Don't warn* was dropped by any mid-session disk reload,
until `108856c` fixed it the same afternoon) —
and 1, 3 and 6 moved because the plain ➖/⬜ hid a real per-frontend split.

Two rows went the *other* way, which is worth saying: **3** and **6** were
understated. The web frontend deliberately preserves selection and Copy/Paste —
its key handler declines Ctrl+Shift+V and stays off the keycodes Ctrl+Insert
uses — and both frontends do have *some* help, just not the fetched-topic kind
this item measures.

> **How the Android column came to be wrong, since it is worth not repeating.**
> The overstated rows all shared a shape: they named specific symbols and file
> paths, which reads as evidence, for code that was never in the repository. A
> citation is not a reading. Everything in that column now names something that
> can be grepped for at `origin/master`, and the rows that are ⬜ say what is
> absent rather than leaving the cell blank.
>
> **A sweep of a column is not a sweep of the document.** Three `c26aeb7`-era
> claims outlived the Android rewrite because they were not in the column: two
> in *Suggested priority order*, which is not a row, and one in a bullet of row
> 4 rather than in that row's per-port paragraphs. Whoever sweeps a column next
> should grep this whole file for the port's name afterwards — the priority list
> and the row bullets are where an old reading hides.

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

### 1. Configurable keyboard map (termcap-style)  — *partial on iOS/macOS; absent on Android*
CP/M is pure ASCII with no function/navigation keys; each terminal defines its own
escape bytes. z80cpmw lets the user bind every special key (Up/Down/Left/Right,
Home/End, Insert/Delete, PageUp/PageDown, F1–F12) to arbitrary byte sequences
written as termcap-style escape strings.
- **Behaviour/spec:** escape syntax `\E` (=0x1B), `\n\r\t\b\f\s`, `\NNN` (octal),
  `^X` (control), `^?` (DEL); names case-insensitive (Ins/Del/PgUp/PgDn accepted);
  empty value unbinds. Defaults are VT220/xterm.
  **A name may carry `Ctrl+`, `Shift+` and `Alt+` prefixes**, stacked in any
  order (`Ctrl+Left`, `Ctrl+Shift+F3`), so a modified key is a binding of its
  own rather than an alias for the plain one. The four Ctrl+arrows are bound by
  default to the xterm modified forms (`ESC[1;5A`…`D`). A modified press with no
  binding falls back to the unmodified one, which is what every modified press
  got before prefixes existed — so this is additive for an existing config.
  `cpmemu` closed the same gap its own way in `0b8dc2b`, hard-coding four
  WordStar sequences; a port adopting this schema gets the general case.
  On Windows `Alt` doubles as the menu key, so an `Alt+` binding is honoured
  only when that exact combination is bound — the plain-key fallback does not
  apply to it. A port whose platform reserves a modifier should do the same. `f1ToCpm` / `f5ToCpm` /
  `ctrlRToCpm` flags decide who receives the keys that double as app shortcuts
  (F1=Help, F5/Shift+F5=Start/Stop, Ctrl+R=Reset); a key claimed by the app is
  swallowed whole and CP/M never sees it. F1/F5 default to the app because CP/M
  has no function keys; `ctrlRToCpm` defaults to **true** because `^R` (0x12) is
  ASCII CP/M really reads, so Reset has no default shortcut and lives on the
  Emulator menu.
  **The full, copy-pasteable spec is `docs/CONFIGURATION.md`** (and the in-app
  Configuration help).
- **Where:** `z80cpmw/Keymap.h` (`keyIdForName`, `nameForKeyId`,
  `validateSequence`, `reservedKeys`), `TerminalView.cpp` (`setKeyBindings`,
  `handleKeyDown`), `Config.h` (`KeyboardConfig`: `f1ToCpm`,`f5ToCpm`,
  `ctrlRToCpm`,`keys`), `MainWindow.cpp` (`rebuildAccelerators`,
  `updateMenuAccelHints`), `SettingsDialogWx.cpp` (`buildKeyboardPage`).
- **Config:** the `keyboard` block in the JSON config — **or Settings →
  Keyboard**, which reads and writes that same block (2026-08-28; not in a
  shipped build yet). Every bindable key with what it sends and a status, a box
  to edit the sequence, Default and Unbind, and the three app-shortcut switches.
  Two details a port copying this should copy too. The list is keyed by
  **resolved id**, so `"Control+Left"` and `"Ctrl+Left"` are one row rather than
  two, and names it cannot resolve are carried through untouched — a
  read-modify-write of a hand-edited file must not delete a line somebody typed.
  And the sequence box is validated on **every keystroke** but committed only
  when the selection leaves the row, because every prefix of a half-typed
  sequence raises its own event and some prefixes are legal on their own.
- **Platform mapping:** the *byte sequences sent to CP/M* are the parity target.
  Mobile maps soft-key/hardware-key events; the CLI maps host-terminal keys. Adopt
  the same config schema (named keys → termcap strings) so configs are portable.
- **Verified port behaviour (2026-08-07):**
  - **cpmdroid (Android)** — ⬜, and the previous ✅ here was the largest of the
    2026-08-07 errors. There is **no key map on Android at all**: no
    `DEFAULT_KEY_BINDINGS`, no `decodeKeySequence`, no `bindingNameFor`, no
    `sendNamedKey`, no `keymap_` preferences, no on-screen key row and no
    Settings → Keyboard Map. Zero grep hits for any of them at `origin/master`.
    `TerminalView.handleKeyDown` is a fixed `when` over keycodes, and the bytes
    it sends are compiled in.
    What that fixed table now covers, as of 2026-08-25, is the VT220 set this
    document specifies: Enter, Backspace, Tab, Esc, the four arrows as
    `\E[A`–`\E[D`, and **F1–F12** as `\EOP`–`\EOS` and `\E[15~`…`\E[24~`.
    The F-keys are new — before that they reached `else -> -1` and were dropped,
    so the old claim that "all twelve F-keys always reach CP/M" was exactly
    backwards. **Ctrl** now covers the whole `'@'`–`'_'` window rather than
    A–Z alone, so Ctrl+`[` Ctrl+`\` Ctrl+`]` Ctrl+`^` Ctrl+`_` Ctrl+`@` and
    Ctrl+Space produce their control bytes; it resolves the key through the
    layout (`KeyEvent.getUnicodeChar` with the Ctrl bits cleared) rather than
    hard-coded keycodes, so it works on a non-US keyboard.
    So the *defaults* now agree with `Keymap.h` and a user gets the same bytes;
    what is missing is the configurability this row is actually about. There is
    still no `f1ToCpm`/`f5ToCpm`/`ctrlRToCpm` equivalent and none is needed —
    F1/F5 are not app shortcuts on Android, and Reset has no Ctrl+R accelerator
    to compete with `^R`, which `setupToolbar` now carries a comment to protect.
  - **ioscpm (iOS/macOS)** *(2026-08-26, build 52)* — ◐, and closer than it was.
    The map moved out to `iOSCPM/Views/KeyMap.swift` in build 51 and it has the
    same termcap escape schema (`KeyMap.expand`; it has no explicit `\^` case but
    its default arm emits the same literal `^`, so every documented escape
    decodes to the same bytes) and a per-key editor. **Build 51 closed the
    F1–F12 gap**: `SpecialKey` is twenty-two cases now, not ten, and its F-key
    bytes are this repo's — `\EOP`..`\EOS` then `\E[15~`, `\E[17~` and up,
    skipping 16 and 22 the way a real VT220 does. Its own tests assert them
    against this table rather than against "something reasonable", which is the
    check that keeps the two maps portable. The `VT52` profile deliberately
    differs: PF1–PF4 as `\EP`..`\ES` and nothing for F5–F12, because a VT52
    program cannot be expecting a VT100 sequence.
    What still differs is the *shape* of the map, not its size. There is no
    modifier concept at all — `SpecialKey` names a key, so `Ctrl+Left` cannot be
    bound separately from `Left`, which is the gap this repo closed for itself in
    `[Unreleased]`. The names are lower-camel raw values (`up`, `pageUp`) rather
    than `Up`/`PageUp`. It is organised as named profiles (`WordStar`,
    `VT100/ANSI`, `VT52`, `Custom`) whose default is still the **WordStar
    diamond** (`^E`/`^X`/`^S`/`^D`), not the VT220 table, and its `VT100/ANSI`
    profile still binds Delete to `\E[3~` where z80cpmw and cpmdroid send `^?`.
    And there is still no on-screen way to press any of them: the twenty-two
    bindable keys need a hardware keyboard. `ioscpm`'s `todo.txt` calls that its
    largest remaining gap, and `4deea96`'s own message notes that build 51 made
    it larger rather than smaller.

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
  (copyable + Open Folder), and the **boot banner** — and, as of the v1.36 core
  sync, **to the guest itself**: `emu_host_file_get_write_name()` reports the
  *effective* destination rather than the requested name, so `W8` prints the data
  folder for a bare name and the redirected `LocalCache` path in an installed MSIX
  build. That reaches users only when the disk images carry the `w8.com` that asks
  (`HBF_HOST_GETNAME`, `0xE8`); see `todo.txt`. This port also defines
  `emu_host_path_caps()` and sets `EMU_HOST_CAP_SAFE_PATHS` — honestly, since the
  bit means a guest path is never used *destructively*, not that it is confined to
  one directory, and open-write here is a plain create-or-replace with no delete
  and no substitution. The core declares that function and does not define it, so
  the assertion can only be made by the backend it is about.
- **Where:** `z80cpmw/emu_io_windows.cpp` (`resolveHostPath`, `isAbsolutePath`,
  `emu_host_file_close_write`, `emu_host_file_get_write_name` /
  `resolveRealPathForDisplay`, `emu_host_path_caps`,
  `emu_io_get_data_folder_display` / `resolveRealPath`). Covered by
  `tests/test_hostfile.cpp` and `tests/test_hbios_hostfile.cpp`, 102 checks.
- **Verified port behaviour:**
  - **romwbw_emu (CLI)** *(2026-08-26)* — **both halves, since `98eb6a1`.** `R8`
    copies the command tail at 0x80 and the backend `fopen`s it verbatim
    (`emu_io_cli.cc`, `emu_host_file_open_read`), with a case-insensitive
    per-component retry (`resolve_path_case_insensitive`) to undo the CCP's
    uppercasing — so any host path works. `W8` is `W8 <cpmname> [hostpath]` now:
    `w8.asm` reads the command tail as well as the default FCB, takes the whole
    rest of the line as the path so a directory name may contain spaces, and
    still falls back to the bare lowercased 8.3 name in the emulator's CWD when
    no path is given. On the backend side `resolve_write_path` resolves the
    parent case-insensitively, canonicalises it and lowercases the leaf it
    creates, and `emu_host_file_get_write_name()` answers with the absolute path
    that was actually opened, so `W8` prints where the file went rather than
    echoing what was typed. The CLI's `--help` describes all of it — see the
    "File transfer (run inside CP/M)" block in `romwbw_emu.cc`.
    `W8` also refuses to hand a host path to an emulator that does not answer the
    `0xE9` capability probe with `CAP_SAFE_PATH`; the CLI's
    `emu_host_path_caps()` returns `EMU_HOST_CAP_SAFE_PATHS`, so the guard is
    invisible here and is what protects an older port that syncs the utility
    without the backend.
    Worth recording why this row read ✅ for years before it was true: the R8
    half is genuinely unrestricted, the W8 half was never checked, and the CLI's
    own `--help` said "Export CP/M file to emulator CWD" the whole time.
  - **ioscpm (iOS/macOS)** *(2026-08-26, build 52)* — `W8` always writes
    `Documents/Exports`, `R8` always reads `Documents/Imports` (no per-transfer
    dialog). As of **v1.4.11 / build 41** an **Import File…** picker (enabled on
    iOS *and* Mac Catalyst) stages an arbitrary-location file into `Imports` for a
    later `R8`; the old opt-in per-transfer picker was removed. So arbitrary-path
    *import* is covered (via staging), but `W8` export still has no
    save-as/arbitrary path. Findability is good: iOS exposes Documents to the
    **Files app** (`UIFileSharingEnabled` + `LSSupportsOpeningDocumentsInPlace`),
    and both platforms have **Open Imports/Exports Folder** menu items. Since
    build 52 `emu_host_file_get_write_name()` answers with the real `Exports`
    path, so `W8` prints where the file went rather than echoing the guest's
    string — the same `HBF_HOST_GETNAME` (`0xE8`) contract this repo implements.
    **The hazard the fixed-folder model was hiding.** `98eb6a1` gave `W8` an
    optional host path and it is sent verbatim; that port stored it unsanitised
    as the export *filename*, and `saveToExportsFolder` then built a destination
    with `appendingPathComponent` — which does not escape `..` — followed by
    `try? fm.removeItem(at:)`. `W8 ANYFILE.TXT ..` therefore resolved to
    `Documents` and deleted it recursively: `Disks`, `Imports` and `Exports`, so
    every disk image the user had downloaded. The `try?` swallowed the error and
    `emu_host_file_close_write()` returns before the Swift layer runs, so the
    guest was told the export succeeded. `R8` had the matching bug on the read
    side: an unsanitised path missed and fell back to **the first file in
    `Imports`**, loading unrelated contents into CP/M under the requested name,
    with a success message on both sides. Build 52 fixes both in three
    overlapping layers — the core reduces the string in
    `emu_host_file_open_write()` through the shared `emu_host_path_basename()`
    new in `romwbw_emu` v1.36, a new `ExportPath` type owns the reduction and
    proves the result lands directly inside `Exports`, and `saveToExportsFolder`
    no longer calls `removeItem` at all, since `Data.write(to:)` replaces a file
    by itself.
    Two things this repo should take from it rather than merely note. First, the
    reason `romwbw_emu` `docs/RELEASE_ORDER_2026-08-25.md` puts the port fixes
    ahead of the disk-image refresh: it is refreshing the images that puts a
    path-capable `W8` in front of every user, so an unfixed port is only safe
    while the old `w8.com` is what ships. Second, containment belongs in the
    layer the capability bit is about — `emu_host_path_caps()`, not the UI above
    it.
    **Two changes since, at `15f48e9`** (2026-08-27), neither of which
    contradicts anything above. `emu_io_ios.mm` now defines
    `emu_host_file_get_read_name()` and returns `""`, which that port had to do
    or stop linking — `hbios_dispatch.cc` calls it unconditionally for
    `HBF_HOST_GETRNAME` and is symlinked into `iOSCPM/Core/`, so the core moved
    under that repo without a file there being touched, exactly as
    `emu_host_path_caps()` did before it. `""` is a legal answer that `emu_io.h`
    names as one, and it is the honest one there: the effective source is known
    only in Swift, where the delegate resolves a leaf against `Imports`
    case-insensitively and then calls `emu_host_file_load()`, which carries
    bytes and no name. **This repo had the same missing symbol and answers
    differently** — `emu_io_windows.cpp` records the path
    `emu_host_file_open_read()` actually resolved and opened — because on this
    backend that path is known. And the **zero-byte `W8` export bug is fixed**
    on iOS: `emu_host_file_close_write()` no longer requires a non-empty buffer
    to reach `WRITE_READY`, and `checkHostFileState()` no longer guards on the
    write-data pointer, which by the shared contract is `nullptr` for an empty
    buffer and so could never answer the question. Both halves were needed;
    either alone still swallowed the export. That is the same divergence
    `cpmdroid` closed in `c06fa58` and the browser backend closed in v1.36, so
    all four ports now create the empty file that this one and the CLI always
    created. Neither change has been compiled — that repo records it as NOT
    COMPILED, for want of Xcode.
  - **cpmdroid (Android)** *(2026-08-25, from `origin/master`; this supersedes
    the 2026-08-07 reading, which described a Files button, an Import File…
    picker and a Share action that **do not exist** — no `res/xml` directory, no
    `FileProvider`, no `ACTION_SEND`, no `OpenDocument` anywhere in the tree)* —
    the arrangement is the fixed-folder one the 2026-07-23 reading described.
    `R8` reads only `Imports/` and `W8` writes only `Exports/`, both under
    `getExternalFilesDir(null)` (`/Android/data/com.awohl.cpmdroid/files/…`),
    and there is **no UI of any kind for either folder**: no path display, no
    picker, no share sheet. So the Android 11+ "`Android/data` is invisible in
    the Files app" trap is live, and this is the port where an exported file is
    hardest to reach — which makes it, not iOS, the weakest cell in this row.
    Containment is real, though, and as of 2026-08-25 it is asserted in the
    right layer. The guest path is reduced to a single leaf by
    `emu_host_path_basename()` inside `emu_host_file_open_read/write()` in the
    C++ shim, and lowercased to match the CLI and browser convention; the Kotlin
    checks remain as a backstop and the write is now containment-checked against
    `Exports` before it happens. `emu_host_path_caps()` returns
    `EMU_HOST_CAP_SAFE_PATHS` on that basis, and
    `emu_host_file_get_write_name()` reports the full `Exports/` destination so
    `W8` can print where the file went — which is the closest thing this port
    has to findability until a UI exists.
    One real bug was fixed on the way: `R8` used to fall back to **the first
    file in `Imports/`** when the requested name was missing, and hand it to
    CP/M under the requested name while printing its usual success line.
    **A second one at `c06fa58`** (2026-08-26): a zero-byte `W8` export produced
    no file at all while telling the guest it had succeeded. Three places each
    read "no bytes" as "no export" and any one of them would have swallowed it
    — `emu_host_file_close_write()` reached `WRITE_READY` only with a non-empty
    buffer, the JNI `nativeGetHostFileWriteData()` returned `null` on a zero
    byte count, and `handleHostFileWrite()` bailed on `data.isEmpty()`. The
    state, not the write-data pointer, now says whether an export is waiting,
    which is the only thing that can: `emu_host_file_get_write_data()` returns
    `nullptr` for an empty buffer by the shared contract. The JNI tests
    `WRITE_READY` alone and deliberately not `WRITING`, so a call made while the
    guest is still handing bytes down can no longer serve a partial export as a
    finished one. An empty CP/M file is a real file — this port and the CLI
    always created it — so this closes a divergence rather than choosing a
    behaviour; `ioscpm` closed the same one at `15f48e9`. Not compiled there
    either: no SDK, NDK or Gradle on the machine it was written on, and the C++
    was built for the host against a stub `jni.h`.
- **Parity targets:** (a) let users reach **arbitrary** host locations within each
  platform's file model — a document picker / `ACTION_CREATE_DOCUMENT`; and (b) at
  minimum, **make exports findable**. Both are still open on both mobile ports.
  (b) used to say it was "done on Android (share sheet plus the paths shown
  in-app)"; that was the `c26aeb7` reading talking, and it is the last claim of
  that reading left in this row — there is no `ACTION_SEND`, no `FileProvider`,
  no `res/xml` and no path display anywhere in `cpmdroid` at `9b68ab1`, and
  still none at `c06fa58` (re-grepped 2026-08-26). The
  nearest thing either mobile port has to (b) is `W8` printing its own
  destination, which both do now. (a) needs a save-as for `W8` —
  `ACTION_CREATE_DOCUMENT` on Android, a document exporter on iOS — before
  either can be called ✅.
  The shared iOS/Mac **`help_file_transfer.md`** was stale (wrong bundle id
  `com.awohl.iOSCPM`, wrong app name "iOSCPM", no mention of Import File…);
  `ioscpm` commit `9a9d7fd` fixed it on 2026-07-23. `cpmdroid` has its own
  Android-worded copy at `release_assets/help_file_transfer.md`, split off the
  same day (`78e6ec6`), so the two no longer share that text — a change to one no
  longer fixes the other.

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
  - **ioscpm (iOS/macOS)** *(re-verified 2026-08-24)* — **pinned**, since build
    42. `EmulatorViewModel.swift` holds a single
    `releaseTag = "v1.4.5"` from which both `catalogURL` and `releaseBaseURL`
    are built, with the reason in a comment (the core reports RomWBW v3.5.1;
    slices from another release print an HBIOS/CBIOS mismatch). Like cpmdroid,
    help deliberately stays on `releases/latest` — but unlike cpmdroid, without
    a bundled fallback, so see item 6.
  - **romwbw_emu (web)** *(2026-08-27, at `a95db9f`)* — **there is no catalog
    here at all**, and the cell in the table above used to read "hardcoded list,
    unpinned; 4 of 5 images ship nowhere", which understated it in the one
    direction that matters. Nothing fetches `disks.xml`, nothing names a release
    tag, and there is no downloaded-state or delete UI: `disk0Select` and
    `disk1Select` in `web/romwbw.html-template` are two hardcoded `<select>`s
    listing the same five names — `hd1k_combo.img`, `z80cpm_tools.img`,
    `hd1k_games.img`, `hd1k_cpm22.img`, `hd1k_zsdos.img` — and the page fetches
    the chosen one by **bare relative URL** (`fetchWithProgress(disk0Selection)`),
    so it resolves next to the page and nowhere else. "Unpinned" is true but
    beside the point: there is no remote to pin to.
    **Nothing puts an image next to the page.** `web/makefile`'s two deploy
    targets (`deploy-romwbw-PRODUCTION-ASK-HUMAN-FIRST` and `deploy-dev`) copy
    the rendered `index.html`, `romwbw.js`, `romwbw.wasm` and `vendor/` — no
    `.img`, and no `.rom` either. `.github/workflows/release.yml`'s staging step
    copies the page, the wasm, `vendor/`, `roms/*.rom` into `roms/` and
    `roms/emu_avw.rom` next to the page — and no `.img`. `web/` itself carries
    none. So it is not four of five that ship nowhere, it is **five of five**,
    in every vehicle: the deb, the rpm, either deploy, and `make serve` out of
    the source tree. Both selects come up preselected — disk 0 on
    `hd1k_combo.img`, disk 1 on `hd1k_games.img` — so the failure is what a
    first-time visitor gets, not something they have to go looking for. It is at
    least *reported*: the loader collects `diskFailures` and puts the HTTP
    status on screen rather than starting a machine with no disk in silence.
    Two smaller facts from the same read. The repository has exactly two images,
    `disks/hd1k_combo.img` and `disks/hd1k_infocom.img`; of the five names the
    page offers, four exist nowhere in the tree, and the one image it does have
    besides the combo is not offered. And the ROM select has the same shape but
    was already fixed on the packaged path only: it offers one ROM,
    `emu_avw.rom`, fetched the same bare relative way, and the release workflow
    stages it beside the page with a comment recording exactly that lesson —
    while the two makefile deploy targets, which nothing checks, still do not
    copy it. `web/emu_romwbw.rom` is tracked and referenced by nothing.

### 6. Remote help system + bundled fallback
In-app help fetched from GitHub, with offline bundled topics.
- **Behaviour/spec:** `help_index.json` + markdown topics downloaded and cached;
  bundled "Getting Started" and "Configuration" topics always available offline; a
  small markdown→text renderer (headers, tables, lists, inline code).
- **Where:** `z80cpmw/HelpWindow.{h,cpp}` and `HelpAssets.{h,cpp}` — the
  state-free half (index parsing, the markdown→text renderer, and the cache)
  was split out on 2026-08-28 so it could be put under test, and is 244 checks.
  (ioscpm and cpmdroid already have help — align the topic set and the local
  fallback.) **The on-disk cache is here now**, and `help_assets::resolveTopic`
  is the one place the order lives: download, then cache, then the copy in the
  binary — the same order as `ioscpm`'s below. The third step is written and
  reaches nothing: this repo still bundles only its own two topics. Bundling the
  seven published ones is blocked on a decision rather than on work, since three
  of them are worded for iOS; `todo.txt` carries it.
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
- **Verified port behaviour (2026-08-25, correcting the 2026-08-07 entry):**
  **cpmdroid has NOT fixed this.** The claim that `app/build.gradle.kts` adds
  the repo's `help/` directory as an assets source dir is wrong — that file
  mentions help nowhere, `app/src/main/assets` contains `emu_avw.rom` and
  nothing else, and the repo's own `help/` holds two files (`help_index.json`
  and `help_quick_start.md`), not seven topics.
  `HelpActivity` fetches `https://github.com/avwohl/cpmdroid/releases/latest/download/help_index.json`
  over HTTP with no bundled copy and no on-disk cache, which is precisely the
  trap described above, still armed. The text that follows described the fix
  that was supposed to land:
  `HelpActivity.loadHelpIndex` still **prefers** the published index — so a
  correction can go out without an app update — but falls back to the bundled
  one, and `HelpTopicActivity` falls back per topic to the bundled file of the
  same name. Help now works offline and survives an unpublished release asset.
- **Verified ioscpm behaviour (2026-08-26, build 52):** help **yes**, fallback
  **yes, since build 51** — the port this trap applied to on 2026-08-24 is out of
  it, and `cpmdroid` is the only one still in. `HelpView.swift` still resolves
  `help_index.json` and every topic through `releases/latest/download/`, which is
  the right way round: a published correction still reaches users without an app
  update. What changed is what sits behind it. The index and all seven topics
  ship inside the app, and the order is **download, then the on-disk cache, then
  the shipped copy** — `offlineIndex` / `offlineContent` and the
  `Bundle.main.url(forResource:)` arms behind them — never the shipped copy
  first. The assets are referenced in place from `release_assets/` with
  `sourceTree = SOURCE_ROOT` rather than copied into the target, so there is no
  second copy to drift from the one that gets attached to a release. That is the
  detail worth taking here: this repo has the same seven topics to bundle and the
  same `release_assets/` problem to avoid.
  One consequence of preferring the published copy survives, and it is not a
  defect in the fallback: `release_assets/help_quick_start.md` was corrected in
  build 49 — it advertised a `Ctrl+E` emulator console that does not exist, which
  is actively harmful because `^E` is WordStar cursor-up — and a user with a
  network keeps getting the stale published text until that file is re-attached
  to the newest release. `ioscpm`'s `todo.txt` carries it as a release chore.

### 7. NVRAM / autoboot / boot string
- **Behaviour/spec:** RomWBW autoboot config via `W` at the boot menu persists;
  "Clear Boot Config" resets it; an optional `bootString` is auto-typed at the boot
  menu. **Note the boot-unit numbering:** with the EMU AVW ROM the on-board RAM/ROM
  disks are units 0 and 1, so the first hard disk is unit **2** — see this repo's
  Getting Started help for the user-facing wording.
- **Where:** `z80cpmw/EmulatorEngine.cpp` (`clearNvramSetting`) and
  `z80cpmw/EmulatorEngine.h` (`setBootString`, inline),
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
  `TerminalView::createFont`; config `window` block. **N/A to mobile** — but not
  to Mac Catalyst, which is a resizable desktop window.
- **Verified ioscpm behaviour (2026-08-24):** **absent.** Nothing in the app
  persists or restores window frame or scene state (no `NSUserActivity`, no
  state-restoration hooks, no stored frame), so a Catalyst window opens at the
  system default every launch. Font size is a menu rather than a grid-derived
  size, so there is no auto-size-to-80×25 either. Tracked in `ioscpm/todo.txt`
  alongside the missing Emulator menu.

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
- **Verified ioscpm behaviour (2026-08-24):** **present**, as a six-step menu
  (14, 16, 18, 20, 24, 28 pt) in `ContentView.swift` — the `Label("Font Size…`
  menu — persisted with `@AppStorage("terminalFontSize")` and applied by
  recreating the terminal view
  on change. A fixed choice list rather than a slider, so Android's stored-value
  clamp problem cannot arise here. There is no pinch-to-zoom.

### 10. Cromemco Dazzler graphics card (optional)
Emulated retro graphics card in a separate window.
- **Verified romwbw_emu behaviour (2026-08-24):** **absent**, not partial — this
  row said ✅ (partial) and there is no Dazzler code in that repo at all. Every
  "Dazzler" string in it is a *comment* on a hook provided **for** a client like
  this one: `handleUnknownPortOut` in `hbios_cpu.h` and the memory-write
  callback in `romwbw_mem.h`. Neither romwbw_emu frontend overrides the hook,
  so unknown ports hit the base no-op. What probably produced the ✅ is the
  web frontend's video/DSKY/sound code, and that is dead: the C++ side emits
  `Module.onVideo*` and `Module.onDsky*` while the page implements
  `Module.onVda*` and `Module.onSnd*` — **zero overlap**, ~200 lines on each
  side that have never executed. That half still stands: `2dbf6f2` looked at it
  and deliberately left it alone. What that commit did fix is the one channel
  that would have complained — `Module.onError`, called by `emu_error()`
  (`src/emu_io_wasm.cc`) and implemented nowhere, so every error the core
  reported went nowhere at all. The page implements it now, to the status line
  and to `console.error`, which is a large part of why the dead wiring above
  survived unnoticed for so long.
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
- **cpmdroid (Android)** — ⬜. There are no profiles: `saveProfile`,
  `loadProfile`, `listProfiles` and `deleteProfile` do not exist, and there is
  no Settings → Configuration Profiles. `SettingsRepository` is a flat
  `SharedPreferences` wrapper over one current setting each (ROM, four disk
  slots, font size, wrap, scrollback, sound, manifest warning, NVRAM). That is
  the same shape as the `romwbw_emu` CLI's single settings file, so this row is
  ◐-at-best on three of the four ports and ✅ only here.

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
- **Verified ioscpm behaviour (2026-08-24):** **present and suppressible.** A
  "Disk May Be Overwritten" alert (grep `ContentView.swift` for that string —
  it has two presentation sites) fires on a write to a catalog disk, and
  Settings has a *Warn on manifest writes* toggle bound to
  `EmulatorViewModel.warnManifestWrites`, persisted in `UserDefaults` with the
  default applied when the key is absent. The alert is informational — it points
  the user at *Save Disk As* rather than offering to cancel the write.

### 13. Terminal emulation (VT100/ANSI + VT52)
The front end **is** the terminal, so its escape-sequence coverage decides which
CP/M software actually runs: WordStar, Zork, TERMDEF, VDE and anything else
full-screen. This belongs in a front-end parity catalog for the same reason
copy/paste does. This repo was **not** the reference for it - `ioscpm` wrote the
parser - but it has now caught up. (This used to credit `cpmdroid` with
extending it; that port's parser turned out to be the thinnest of the four.)
- **Behaviour/spec (what `ioscpm` implements, and the target):** VT100/ANSI
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
  - **ioscpm** *(2026-08-26, build 52)* — `iOSCPM/Views/EmulatorViewModel.swift`.
    The origin of the parser: full VT52, scrolling region, answerbacks, deferred
    autowrap, charset consumption. **Build 51 closed the gap this entry used to
    name.** `@` (ICH), `P` (DCH), `X` (ECH), `S` (SU) and `T` (SD) are all
    implemented, and DECAWM (`?7`) and DECTCEM (`?25`) are acted on rather than
    parsed and dropped — both returning to their power-on state on cold boot, so
    a guest that hides the cursor and dies does not leave it hidden for the next
    session. SU routes through `scrollUp()` only when the region is the whole
    screen and through `scrollRegion()` otherwise, which is the rule LF already
    followed: lines pushed out of a status-line window were never history.
    One difference remains and it is deliberate on that side: the new finals
    blank with a *default* cell rather than the current SGR background, matching
    the rest of that port's erase family, where this repo paints the current
    background everywhere. That port files changing it as a decision for the
    whole family at once, on the grounds that doing it to half would be worse
    than being consistently wrong. **Settled 2026-08-27, in this port's
    favour:** an erase blanks with the current SGR background, which is strict
    VT and xterm behaviour and what a program that sets a colour and clears is
    asking for. Nothing changes here; the erase family in
    `EmulatorViewModel.swift` moves, including the `@ P X S T` finals, and it
    needs an `ioscpm` release. **That move has landed upstream since**, in
    `0165dac`, so it is no longer a difference — read on 2026-08-28 at
    `0dbab43`, two commits past the `15f48e9` this column was carried forward
    to. Treat this as a reading of `applySGR` and the erase family, not of the
    column: the rest of the cells below still stand at build 52.
    That reading turns up a different difference, and it is the one this repo
    spent 2026-08-28 closing in itself: **`applySGR` there has no bright half.**
    The whole switch is `0`, `1`, `22`, `7`, `27`, `30...37`, `40...47` and
    `default: break`, so `ESC[91m` leaves the attribute byte alone and the text
    draws in whatever was current — exactly what this repo did until `978b623`.
    `cpmdroid`'s `TerminalView.kt` has carried `p in 90..97` since its own ANSI
    fix, which makes `ioscpm` the one port without it. That switch also has no
    per-cell face: `1` sets the CGA intensity bit and nothing records bold,
    underline or blink, so this repo's `TCELL_*` flags and four-face paint path
    have no counterpart there. `todo.txt` records both for whoever is next at a
    Mac.
    Parser input **is** bounded, since build 49: `maxCSIParams` 16 and
    `maxCSIParamDigits` 6, matching cpmdroid, with leading zeros dropped so
    zero-padding cannot spend the digit budget. Build 49 also made SGR 7 a
    render-time toggle instead of an in-place nibble swap, so SGR 27 restores
    the original colours instead of resetting to white-on-black.
  - **cpmdroid** — ⬜, and this is the row where the 2026-08-07 reading was
    furthest from the code. `app/src/main/java/com/awohl/cpmdroid/TerminalView.kt`
    at `origin/master` has **no VT52, no DECSTBM, no DECSC/DECRC, no
    answerbacks, no private-mode handling, no parameter bounds and no `P @ X S
    T`** — none of which it could have "ported from the above", since this
    repo's own item 13 says the flow ran the other way. What is there is a
    three-state parser whose whole CSI dispatch is `H f A B C D J K m`; SGR
    handles foreground only (`0`, `30`–`37`, `90`–`97`, no bold, no background,
    no reverse) and resets to green rather than white; and ESC followed by
    anything but `[` is discarded, which is what makes every sequence above
    unreachable rather than merely unimplemented.
    In the normal state it handles ESC, CR, LF, BS and BEL. **TAB was dropped
    entirely** until 2026-08-25 — the `else` arm prints 0x20 and up, so 0x09
    matched nothing — which collapsed every tabbed layout, including RomWBW's
    own tab-indented boot banner and `DIR`. It now advances to the next
    8-column stop like the other ports. FF is still discarded.
    So the mobile ports did not jointly lead this row: **`ioscpm` did**, and
    z80cpmw's item 13 work came from there.
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
    through a flag separate from the blink phase. Covered by the conformance
    suite described below.

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

    Covered by a headless conformance suite which **is** committed here, as
    `tests/test_vt52.cpp`, and which is at **516 checks** — it was the 73 that
    CHANGELOG [1.0.20] describes when that was written, and 268 when it first
    landed in `tests/`. It drives the terminal through the public interface
    only: cursor state is read back with `ESC [ 6 n`, which puts the answerback
    under test rather than assuming it, and screen content through `cellAt()`.
    `tests\run_tests.bat` runs it first of **six suites, 1020 checks**, and the
    one beside it is not a model check at all — `tests/test_render.cpp` opens a
    real window, drives it with real bytes, asks the DWM for it with
    `PrintWindow(PW_RENDERFULLCONTENT)` and reads the pixels, 50 checks over the
    colours and the faces below. Parser input is bounded here —
    `MAX_CSI_PARAMS` 16 and `MAX_CSI_PARAM_DIGITS` 6 in `TerminalView.cpp`,
    with intermediates consumed rather than accumulated.

    **Per-cell attributes closed 2026-08-28, and the entry that stood here was
    wrong twice over.** It named reverse video as missing alongside bold,
    underline and blink; reverse was already implemented when that was written,
    and about nine checks pinned it — `m_reverse`, `m_savedReverse` and
    `swapAttrNibbles()`. It stays deliberately **out** of the flags byte,
    because it is resolved into the colour nibbles at the write, which is what
    makes SGR 7 and 27 exact inverses. What was really absent was the other
    three, and both halves have landed: `TerminalCell::flags` carrying
    `TCELL_BOLD` / `TCELL_UNDERLINE` / `TCELL_BLINK` off SGR 1/4/5/6/22/24/25
    (`480edcb`), and a paint path that draws them (`29d3438`) — `m_fonts[4]`
    indexed by bold and underline, because GDI cannot turn weight or underline
    on for a single `TextOut`, with `SelectObject` called only where a cell asks
    for a different face, and blink sharing the cursor's existing 500 ms timer
    rather than opening a second phase free to drift against it. An erase zeroes
    the flags while keeping the colours: underline and blink are visible on a
    space, so carrying them would mean `ESC[4m ESC[2J` underlines all 2000
    cells. There is no italic face because nothing can ask for one — the flags
    byte has three bits and SGR 3 is not among them.

    **Fixed since:** `clear()` no longer resets the attribute and escape state
    on `ESC [ 2 J`. Erasing and resetting are separate functions now —
    `eraseScreen()` for `ESC [ 2 J` and VT52 `ESC E`, `clear()` for the machine
    reset — which also means a reset finally *does* reset VT52 mode, DECAWM,
    DECTCEM and the scrolling region, all of which it used to leave alone
    precisely because `ESC [ 2 J` shared the path. Note that `ESC [ 2 J`
    deliberately still preserves the scrolling region: `ioscpm`'s
    `clearTerminal()` resets it, and that is `ioscpm`'s bug, not a gap here.

    **Also fixed 2026-08-28:** SGR **90–97** and **100–107** were not handled at
    all — they fell through `applySGR()`'s default and left the attribute byte
    alone, so `ESC[91m` from a fresh reset drew in CGA 7. Measured by
    `tests/test_render.cpp` on its first run, not inferred. The bright half is
    the same ANSI index with the intensity bit set, which is the bit SGR 1
    already sets, so `22` dims a bright colour and a `3x` after a `9x`
    deliberately does not; `100–107` fold onto the plain background, because the
    background nibble is three bits and the fourth is blink on real hardware,
    and a wrong shade beats a cell that starts strobing.
  - **romwbw_emu** *(re-verified 2026-08-24)* — the CLI delegates to the host
    terminal, but not transparently: `emu_console_write_char` in
    `emu_io_cli.cc` does `ch &= 0x7F` and then drops every CR, not
    just the CR of a CR LF pair — so a guest returning to column 0 without a
    newline (progress counters, status-line redraws) overwrites nothing, and
    8-bit output is gone before the tty sees it.

    The web build loads **xterm.js 5.3**, which *is* a far more complete VT than
    any native front end, and since `2dbf6f2` the app no longer starves it.
    `Module.onConsoleOutput` (the `Module.onConsoleOutput =` assignment in
    `web/romwbw.html-template`) hands every byte to `term.write()` unchanged,
    with LF the single exception: it is written as CR LF, because a CP/M guest's
    bare LF means new line and xterm.js would otherwise leave the column where
    it was. The filter it replaced passed only CR, LF, BS, ESC and `0x20–0x7E`,
    dropped **TAB, BEL, FF, every other control byte and everything ≥ 0x7F**,
    and rewrote BS as `\b \b` — a *destructive* backspace, so a guest moving the
    cursor left erased a character. CSI sequences survived that only because
    their bodies happen to be printable ASCII. The row was ✅ on the strength of
    the library while the wiring was what decided it; the wiring now agrees with
    the library.
- **Parity target:** the mobile ports' coverage, i.e. run WordStar and Zork
  without the screen breaking up. **That port is done** — `TerminalView.kt` /
  `EmulatorViewModel.swift` were pulled back into `TerminalView.cpp` in 1.0.20,
  which is the code in the current Store **1.0.22** and sideload
  **1.0.22-beta** packages. **Nothing keeps this repo's row at ◐ any more.**
  The single item that did — no per-cell attribute beyond the packed CGA byte —
  closed on 2026-08-28 in `480edcb` and `29d3438`; the row is ✅ in the tree,
  and in no shipped package. The other half of the
  residue — `clear()` resetting the attribute and escape state — is fixed, along
  with three SGR bugs the audit for it turned up: reverse video swapped the
  attribute nibbles in place and so could not round-trip, setting a colour
  masked out the bold bit, and `ESC [ m` left the reverse flag set. A fourth,
  found in a later pass and larger than those three, is also fixed: SGR colour
  parameters carry ANSI colour numbers and were stored straight into a
  CGA-ordered attribute byte, so four of the eight colours drew as a different
  colour entirely. A fifth is the bright half, `90–97` and `100–107`, which was
  not implemented at all; it took a suite that reads pixels to find it, since
  the model was self-consistent without it.

---

## Per-port gap snapshot (verify before acting)

✅ present · ◐ partial · ⬜ missing · ➖ N/A or host-provided · ❓ verify

Android `cpmdroid` is as of **`origin/master`, 2026-08-25, read from source** —
the whole column, after the `c26aeb7` citations turned out to describe code that
was never pushed — and every absence claim in it was **re-verified against
`c6756af`** on 2026-08-27, with none falsified. See the note at the head of this
file.
The **iOS/macOS column was re-read from `ioscpm` source on 2026-08-26**, at
**build 52** (`49851aa`), from a checkout on this machine — every row, not only
the ones that changed — and carried forward to **`15f48e9`** on 2026-08-27,
which adds two facts to row 4 and falsifies nothing. Builds 51 and 52 both
landed after the 2026-08-24 reading this replaces; see the note at the head of
this file.
The **Linux/Web `romwbw_emu` column was re-read row by row at `a95db9f`** on
2026-08-27, its first recorded reading; the 2026-08-24 sweep it replaces wrote
down no commit. Twelve of its thirteen cells stood as written and row 5 did not
— see item 5.

**Which commit each column was read at, and what reports the drift.** A column
is only as current as the last person who read that tree, and three of the four
trees moved the same afternoon this document was written. The commits actually
read are recorded below, and `tools/check-sibling-drift.sh` compares them with
the checkouts beside this one, lists what has landed since, and exits non-zero
if anything has - including a recorded commit that is not an object in the tree
it names, which is the `c26aeb7` failure caught mechanically instead of by
argument. Re-read what it reports, correct the column, then update this block.

It compares against **`origin`**, not against the local checkout. It used to
compare against local `HEAD`, and that let a stale checkout certify a column as
current: on 2026-08-27 the `ioscpm` line read "current" while the checkout on
this machine was two commits behind `origin/main`, so the column was being
blessed against a tree nobody else had. The tip is now
`refs/remotes/origin/HEAD` (falling back to `origin/main`, then
`origin/master`), a local `HEAD` behind that tip is reported in its own right,
and a checkout with no origin ref at all fails rather than being quietly
measured against itself. It still does not fetch by default - a remote-tracking
ref is only as fresh as the last `git fetch` in that tree, so each line prints
when that was; `--fetch` updates them first and is the only thing the script
does that writes to a sibling.

```sibling-readings
ioscpm     15f48e9  2026-08-27
cpmdroid   c6756af  2026-08-27
romwbw_emu a95db9f  2026-08-27
```

What each of those three readings is, because they are not the same kind of
thing:

- **`ioscpm` `15f48e9`** - a delta check, not a re-read. Build 52's full reading
  (`49851aa`, 2026-08-26) still stands underneath it; `6b1b731` changes no
  source at all, and `15f48e9` changes two things in row 4, both recorded there.
  Two commits past it, `0165dac` and `0dbab43`, were read on 2026-08-28 for
  **row 13 alone** - the erase family and `applySGR`, nothing else - and both
  are recorded in that row. The tip above deliberately stays at `15f48e9` so the
  drift script keeps reporting that column as moved: reading one function is not
  reading a column, and this line is what certifies one.
- **`cpmdroid` `c6756af`** - the 2026-08-25 full reading (`9b68ab1`) plus a
  re-verification: every *absence* claim in the Android column was re-checked
  against the tree at `c6756af` and every one stands. `c6756af` is documentation
  only; `c06fa58` is the source change, and it is in row 4.
- **`romwbw_emu` `a95db9f`** - a row-by-row re-read of the whole column, which
  it had not had since 2026-08-24, fifteen commits earlier. That sweep never
  wrote down what it read, so this line said `unknown` until now. Twelve of the
  thirteen cells still stood as written; row 5 did not, and was rewritten from
  the page, the makefile and the release workflow rather than adjusted.

| Feature | iOS/macOS `ioscpm` | Android `cpmdroid` | Linux/Web `romwbw_emu` |
| --- | :---: | :---: | :---: |
| 1. Configurable keymap (termcap) | ◐ (22 keys incl. F1–F12 since build 51; no modifier bindings, lower-camel names, WordStar default, no on-screen key row) | ⬜ (no map at all; fixed table, now VT220 incl. F1–F12 and full Ctrl window) | ➖ CLI (host terminal) · ◐ web (xterm.js fixed map, not configurable) |
| 2. Scrollback | ✅ | ✅ (Settings slider incl. Off since 2026-08-25; drag instead of wheel) | ➖ CLI (host terminal) · ◐ web (xterm.js default buffer, no option set) |
| 3. Mouse/native Copy-Paste | ✅ | ✅ (control strip; Copy takes the scrollback since 2026-08-25, no selection) | ➖ CLI (host terminal) · ✅ web (xterm.js selection) |
| 4. R8/W8 arbitrary host paths | ◐ (R8 via Import File…; W8 fixed to `Exports`, and reports it since build 52) | ⬜ (both folders fixed, no picker, no share, no path UI) | ✅ CLI (R8 any path; W8 `<cpmname> [hostpath]` since `98eb6a1`) · ✅ web (picker/download) |
| 5. Disk catalog + **pinned** tag | ✅ / ✅ pinned (`v1.4.5`) | ✅ / ✅ pinned (`v1.4.5`) | ➖ CLI (local paths only) · ⬜ web (no catalog and no tag: a hardcoded five-name `<select>` fetched beside the page, and nothing ships a single `.img` — neither deploy target nor the release workflow — so all five 404, both defaults included) |
| 6. Help system + offline fallback | ✅ / ✅ bundled since build 51 (download, cache, then the shipped copy) | ✅ / ⬜ (fetches `releases/latest`, nothing bundled, no cache) | ◐ both (usage text / static panel, no topics — so no `releases/latest` trap either) |
| 7. NVRAM autoboot / bootString | ✅ | ✅ NVRAM / ⬜ bootString | ✅ CLI (`--boot`, NVRAM persisted) · ◐ web (set/clear, never read back) |
| 8. Window state / DPI | ⬜ (Mac Catalyst) | ➖ | ➖ |
| 9. Font size setting | ✅ (menu, 14–28pt) | ✅ (Settings slider, 8–24pt) | ➖ |
| 10. Dazzler | ⬜ | ⬜ (explicit no-op stubs) | ⬜ (no Dazzler code; the core only offers the hooks this repo uses) |
| 11. Config profiles | ⬜ | ⬜ (flat SharedPreferences, no named profiles) | ◐ CLI (one JSON settings file, v1.34; no named profiles) · ◐ web (one UI selection set) |
| 12. Manifest write warning | ✅ (suppressible) | ✅ (suppressible, once per session) | ➖ CLI · ✅ web (*Don't warn* kept across a reload since `108856c`) |
| 13. Terminal emulation (VT100 + VT52) | ✅ | ⬜ (CSI `H f A B C D J K m` only; no VT52, no DECSTBM, fg-only SGR) | ➖ CLI (host terminal; output drops CR, masks to 0x7F) · ✅ web (output filter fixed in `2dbf6f2`) |

**z80cpmw's own row 13 became ✅ on 2026-08-28**, which makes every row in this
document ✅ for z80cpmw — the other twelve by construction, this one on the
evidence in item 13. It was ◐ for one stated reason, no per-cell attribute
beyond the packed CGA byte, and that closed in `480edcb` and `29d3438`. Read the
✅ as "in the tree": the shipped **1.0.22** and **1.0.22-beta** packages are the
1.0.20 parser, without the attribute work and without the bright half of the
palette.

**One caveat spans the whole web column.** `xterm.js`, its CSS and the fit addon
are three jsdelivr `<script>`/`<link>` tags in `web/romwbw.html-template` (grep
it for `cdn.jsdelivr.net`) with no vendored copy and no SRI, and `release.yml`
packages only
`romwbw.html`/`.js`/`.wasm`. So offline — or from an installed deb — `new
Terminal(...)` throws at top level and there is **no terminal at all**: rows 2, 3
and 13 are ✅/◐ only for a browser with internet access. romwbw_emu's `todo.txt`
tracks vendoring the three files, which would close the SRI hole and the offline
gap together.

## Suggested priority order for each GUI port

1. **Terminal emulation (#13)** — decides which software runs at all, so it
   outranks everything else. **All three ports now implement it**: z80cpmw
   caught up last, and the three parsers agree on VT52, the scrolling region,
   deferred autowrap, the editing finals and the answerbacks. What differs is
   detail, listed per port above.
2. **Configurable keymap (#1)** — biggest remaining UX win, fully specced in
   `docs/CONFIGURATION.md`, and portable as a config schema. For iOS/macOS the
   F1–F12 half landed in build 51; what remains is modifier bindings (`Ctrl+Left`
   as a binding distinct from `Left`), the canonical key names, and an on-screen
   way to press any of the twenty-two — which build 51 made *more* pressing, not
   less, since there are now twelve more keys with no touch affordance. **There
   is no worked example in the family to copy.** This list used to point at
   Android's `buildKeyRow` in `MainActivity.kt` backed by
   `TerminalView.sendNamedKey`; neither symbol exists in `cpmdroid` at
   `9b68ab1`, nor at `c06fa58` (re-grepped 2026-08-26). Its `setupControlStrip`
   is Ctrl, Esc, Tab, Copy and Paste, and nothing else. That pair was the last
   of the `c26aeb7` reading left in this list: `944cf9f` rewrote the Android
   *column*, and this list is not a column.
3. **Scrollback (#2)** — small, self-contained, high user value; spec in
   `TerminalView.cpp`. Done on iOS/macOS and Android.
4. **Arbitrary-path R8/W8 (#4)** within the platform's file model. On iOS/macOS
   the import half is covered by staging through **Import File…** and what is
   left is the export half — a document exporter. On Android *neither* half has
   a UI: no import picker, no `ACTION_CREATE_DOCUMENT`, no path shown anywhere,
   which is what makes it the weakest cell in that row.
5. **Pin the disk catalog to an explicit release tag (#5)** to stop HBIOS/CBIOS
   version drift. **Done on both** — Android and iOS/macOS are each pinned to
   `v1.4.5`. What is left is the other half of the same trap: help still
   resolves through `releases/latest`, which is only safe with a bundled
   fallback. iOS/macOS gained one in build 51; **Android has none**, and this
   repo has only its own two topics (#6).
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
