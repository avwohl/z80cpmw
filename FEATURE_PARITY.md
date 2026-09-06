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
**It IS in a shipped build now, and this paragraph said otherwise until
2026-09-06.**  Both commits are ancestors of `dbd53b1`, the 1.0.23 build, so
the attribute work reached the Store and row 13's tick describes software a
user can install.  The sideload channel is a different answer: **1.0.22-beta**
is still the 1.0.20 parser, and no `-beta` has been cut since.

**The tree is still ahead of the Store, by much less than this paragraph used
to say.**  `Version.h` reads **1.0.25**, 1.0.24 is packaged and unsubmitted, and
**seven** commits have touched `z80cpmw/` since `dbd53b1`.  The enumeration that
stood here - a download thread that outlived its dialog, the catalog's shutdown
ownership, a Keyboard page in Settings, three Settings controls that read and
wrote nothing, a downloader hidden at 200% DPI, a Reset confirmation, the seven
help topics bundled into the binary and a refusal to keep a truncated download -
was measured from the 1.0.22 build, which is **31** commits back, and most of it
shipped in 1.0.23.  It is not repeated, because repeating it would assert the
same gap from a baseline two releases stale.  What is needed is a re-read of
this column, which `tools/check-sibling-drift.sh` has been asking for.

**And note how that Store number is known.** Unlike `ioscpm`, which has
`tools/check-store-version.sh` and measures the Microsoft equivalent of it,
nothing here queries the Store: **1.0.23** is this repository's own record
(`CHANGELOG.md`, `211488b`, 2026-09-04), which is an assertion rather than a
measurement.  `shipped:1.0.23` in the block below is only as good as that.

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

**Then, on 2026-08-29, that column moved again and much further, in two rounds
on two machines.** Rows **4**, **6** and **13** were re-read against the port
and all three are corrected below; each had been the port's weakest cell in its
row and none of them is now. Row 4 went ⬜ → ✅ (`71465cb`, watched on an
emulator), row 6 went ⬜ → ✅ on the fallback half (`1f70c6b`, `aee7276`), and
row 13 — the row this document said the Android parser was "the thinnest of the
four" in — went ⬜ → ✅, with a qualifier stated in the row: that work is
compiled and has never been run.

**Three rows are not a column, so the `sibling-readings` line for `cpmdroid` is
deliberately NOT advanced**, and the drift script will keep reporting that
column as moved. That is the same rule the `ioscpm` line is held to a few
paragraphs down: reading three rows is not reading a column, and that line is
what certifies one. Rows 1, 2, 3, 5, 7, 8, 9, 10, 11 and 12 have not been
re-read since 2026-08-27.

**The iOS/macOS column was re-read in full on 2026-09-06, at `af0b9b2` —
build 61, the commit the App Store's 1.5.1 was built from.** Reading it at a
SHIPPED commit rather than at the tree tip is the point: builds 62-65 exist in
`ioscpm` and none has ever been compiled, so a tick taken from HEAD would
describe software nobody can install. Seven of the thirteen rows changed and
every one in the same direction — the column understated what ships. Six of them
had gone unrecorded because the work landed in `8e7587f`, which is still stamped
build 58, the same number as the commit the previous reading was taken at, so no
version number said the tree had moved; row 5 was simply wrong, still naming a
`v1.4.5` pin that build 59 had moved to `v1.4.12`.

It supersedes the 2026-08-26 reading at build 52 (`49851aa`), which superseded
2026-08-24 at build 50 — right when written and stale within the day, build 51
(`4deea96`) landing that afternoon and build 52 (`bb5543f`) the next morning.

What the 2026-08-24 pass corrected stands. Item 5 said the disk catalog was
unpinned when it has been pinned to `v1.4.5` since build 42; item 6 claimed the
help system outright when the offline fallback that item exists for was absent;
items 8, 9 and 12 were unverified `❓` when window state was missing (it is not
any more — see item 8) and font size and the manifest warning are both present; item 13 gained the parser bounds
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
  `2dbf6f2`, hours after this sweep was written - but only the page half: the
  wasm backend still masks to 0x7F and drops every CR, so the row is ◐, not
  ✅.)*
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
- **Config:** *(re-read 2026-09-06 at `dbd53b1`, the 1.0.23 build)* the
  `keyboard` block in the JSON config — **or Settings →
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
<!-- cites: cpmdroid -->
<!-- cites-elsewhere: Keymap.h -->
<!-- cites-withdrawn: DEFAULT_KEY_BINDINGS decodeKeySequence bindingNameFor sendNamedKey keymap_ f1ToCpm f5ToCpm ctrlRToCpm -->
  - **cpmdroid (Android)** — ⬜, and the previous ✅ here was the largest of the
    2026-08-07 errors. There is **no key map on Android at all**: no
    `DEFAULT_KEY_BINDINGS`, no `decodeKeySequence`, no `bindingNameFor`, no
    `sendNamedKey`, no `keymap_` preferences, no on-screen key row and no
    Settings → Keyboard Map. Zero grep hits for any of them at the shipped
    `35873d0`, nor at `origin/master`.
    `TerminalView.handleKeyDown` is a fixed `when` over keycodes, and the bytes
    it sends are compiled in.  None of it is reachable without a hardware
    keyboard: the on-screen control strip is Ctrl, Esc and Tab (plus Copy and
    Paste), so no F-key, arrow or navigation key can be pressed at all on a
    tablet with no keyboard attached - which is the gap the ioscpm cell in this
    row now closes with its three-page key row.
    What that fixed table covers - all of it shipped in versionCode 27 - is
    the VT220
    set this document specifies: Enter, Backspace, Tab, Esc, the four arrows as
    `\E[A`–`\E[D`, **F1–F12** as `\EOP`–`\EOS` and `\E[15~`…`\E[24~` out of
    `sendFunctionKey`, and the whole navigation cluster - `\E[H`, `\E[F`,
    `\E[5~`, `\E[6~`, `\E[2~` and `^?` for Forward Delete, off
    `KEYCODE_MOVE_HOME`, `KEYCODE_MOVE_END`, `KEYCODE_PAGE_UP`,
    `KEYCODE_PAGE_DOWN`, `KEYCODE_INSERT` and `KEYCODE_FORWARD_DEL`.  Ctrl+arrow
    is a binding of its own there too - `sendArrow` emits `[1;5` and the final
    byte when Ctrl is held - so all three ports send the same four sequences by
    default, though they differ on whether the dialect gates them.
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
<!-- /cites -->
<!-- cites: ioscpm -->
<!-- cites-elsewhere: af0b9b2 -->
  - **ioscpm (iOS/macOS)** *(re-read 2026-09-06 at `af0b9b2`, build 61 — the shipped 1.5.1)* — ◐, and closer than it was.
    The map moved out to `iOSCPM/Views/KeyMap.swift` in build 51 and it has the
    same termcap escape schema (`KeyMap.expand`; it has no explicit `\^` case but
    its default arm emits the same literal `^`, so every documented escape
    decodes to the same bytes) and a per-key editor. **Build 51 closed the
    F1–F12 gap**: `SpecialKey` went from ten cases to twenty-two, and stands at
    twenty-six since the Ctrl+arrows, and its F-key
    bytes are this repo's — `\EOP`..`\EOS` then `\E[15~`, `\E[17~` and up,
    skipping 16 and 22 the way a real VT220 does. Its own tests assert them
    against this table rather than against "something reasonable", which is the
    check that keeps the two maps portable. The `VT52` profile deliberately
    differs: PF1–PF4 as `\EP`..`\ES` and nothing for F5–F12, because a VT52
    program cannot be expecting a VT100 sequence.
    **The modifier gap closed on 2026-08-27 in `0165dac`, and this document did
    not notice**, because that commit was read for row 13 alone - the erase family
    and `applySGR`, nothing else.  `SpecialKey` carries `ctrlUp`, `ctrlDown`,
    `ctrlLeft` and `ctrlRight`, and `unmodifiedBase` and `controlModified` are
    mutual inverses, so a modified slot that was never given a binding falls back
    to the plain key - which is what keeps a Custom profile saved before those
    slots existed sending what it always sent.  An *empty* binding is not the same
    as an absent one and deliberately sends nothing.  What still differs is the
    *shape* of the map: it is the four arrows and nothing else, so there is no
    general modifier schema and no way to bind a Shift- or Alt-modified key at
    all.  The default bytes agree with this repo's in the `WordStar` and
    `VT100/ANSI` profiles (`\E[1;5A`…`\E[1;5D`) but deliberately not in `VT52`,
    which binds them to the plain VT52 arrows and has a test asserting it.  And on
    Mac Catalyst the four resolve but never fire: WindowServer takes Ctrl+arrow
    for Mission Control before the app is asked, which the source says in as many
    words.  The names are lower-camel raw values (`up`, `pageUp`, `ctrlUp`) rather
    than `Up`/`PageUp`/`Ctrl+Up`. It is organised as named profiles (`WordStar`,
    `VT100/ANSI`, `VT52`, `Custom`) whose default is still the **WordStar
    diamond** (`^E`/`^X`/`^S`/`^D`), not the VT220 table, and its `VT100/ANSI`
    profile still binds Delete to `\E[3~` where z80cpmw and cpmdroid send `^?`.
    There IS now an on-screen way to press them, which is what this entry
    said there was not: `8e7587f` added `KeyRowLayout` and `SpecialKeyRow`, three
    pages (Nav, Fn, Ctrl) under the terminal reaching all twenty-six bindable
    slots with no hardware keyboard, suppressible from Settings → Preferences and
    carried in a saved profile. It presses through `sendSpecialKey` → the same
    `KeyMap.bytes(for:)` the hardware path uses, so the map still means one thing,
    and `KeyMapTests` asserts the row offers every real key and nothing else. It
    is in the shipped build 61. On Catalyst it is the ONLY way to send Ctrl+arrow,
    which WindowServer takes before the app sees it. What remains ◐ is the map
    itself, not the reach: no modifier bindings beyond the four Ctrl arrows.

<!-- /cites -->
### 2. Scrollback history  — *new in Windows; iOS/macOS reached it at ioscpm build 57 (2026-09-02), not build 43; Android reached it the same day in `e9436a5`, and keeps history across a cold boot on purpose*
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
<!-- cites: cpmdroid -->
<!-- cites-elsewhere: startEmulator onEmulatorReset resetScrollback scrollRegion reset -->
<!-- cites-withdrawn: scrollbackLinesEdit applyTerminalSettings sendBytes replyListener snapToLive resetTerminal historyIsContiguous visibleHistorySize -->
- **Verified Android behaviour — re-read 2026-09-02, and the 2026-08-07 reading
  it replaces was substantially fiction.** Nine symbols the old block cited exist
  nowhere in cpmdroid, in the tree or anywhere in its git history:
  `scrollbackLinesEdit`, `applyTerminalSettings`, `sendBytes`, `replyListener`,
  `userScrollUp++`, `snapToLive`, `resetTerminal`, `historyIsContiguous` and
  `visibleHistorySize`.  Four of the claims they carried were the **opposite** of
  what the code does.  What follows was re-read against the tree with grep, one
  citation at a time; every symbol below resolves.
  - **Matches.** Capacity is a setting — `SettingsRepository.DEFAULT_SCROLLBACK_LINES`
    = **1000**, offered as `SCROLLBACK_CHOICES` (0, 100, 250, 500, 1000, 2000, 5000,
    10000), where `0` disables.  It is a choice list, not the slider or edit field
    the old block described.  Capture happens at a single scroll-off choke point —
    `scrollUp`, which does the four `addLast` calls; `scrollRegionUp` delegates to
    it and captures nothing itself, so the old block named the wrong function.  The
    guard is `scrollTop == 0 && scrollBottom == rows - 1`: the region must be the
    **whole screen**, not merely start at row 0, so a status-line layout does not
    push its region lines into the history.  That is the same shape as this
    repository's `scrollRegionUp` and as ioscpm's `scrollRegion` since build 57 —
    all three now agree, and cpmdroid always did.  **Shift+PageUp/PageDown** move one page and **Ctrl+Home/End** jump to
    oldest/live, exactly the Windows chords, and **plain PageUp/PageDown still go to
    CP/M** — `KEYCODE_PAGE_UP` branches on `isShiftPressed` and otherwise calls
    `sendEscapeSeq("[5~")`.  `copyScreenToClipboard` takes history **and** screen,
    iterating `historyChars` before `screenBuffer`.
  - **Matched since the afternoon of 2026-09-02.** The three defects this block
    listed earlier the same day were all fixed later that afternoon, and are
    re-read here at the commit this column now records.  The view stays anchored as output arrives:
    `processOutput` no longer touches `userScrollUp` at all, and `scrollUp`
    advances it by one for each line it captures while the reader is in history -
    the same anchoring z80cpmw and ioscpm do, and its comment names both.  The
    cursor is drawn only at the live bottom: `drawCursor` is reached under
    `liveRow == cursorRow && scroll == 0`.  And `onDraw` has one drawing path
    instead of two, so history renders with the soft keyboard up; `maxScrollLines`
    counts the live rows a short viewport hides as scrollable content, so a fresh
    boot with nothing in history can still be scrolled to the top of its own
    screen.  What snaps the view back to live is now a key rather than output:
    `sendChar` clears `userScrollUp`, which covers typing and paste -
    `pasteFromClipboard` goes through it - while `sendAnswerback` deliberately does
    not, so a terminal query cannot yank the user out of history.
  - **A deliberate divergence, not a defect — and the old block had this backwards
    too.** History is not cleared on a cold boot, and that is a decision `clear()`
    documents at length: "The scrollback itself is deliberately kept... losing the
    user's history is a product decision, not part of putting the terminal back to
    power-on."  It even names how the siblings differ — both drop it at the CALL
    SITE (this repository's `startEmulator` and `onEmulatorReset` call
    `resetScrollback` beside `clear()`; ioscpm's `reset()` empties its buffer) — which
    here would be `MainActivity`.  So the spec line "history cleared on emulator
    start/reset" is a place the three ports genuinely disagree on purpose, not a bug
    in one of them, and the old block's claim that cpmdroid cleared it was wrong in
    the opposite direction from everything else here: it credited the port with
    behaviour it had considered and rejected.
  - **Genuine platform differences.** No mouse wheel — the touch equivalent is
    **drag** (`onTouchEvent`; drag down reveals older lines), so the "3 lines/notch"
    part has no counterpart.  The page/jump chords need a hardware keyboard, as item
    1 notes.
  - **Withdrawn.** The old block's "capped at 4000 lines / 200000 characters"
    clipboard limit, and the Binder-transaction reasoning under it, describe nothing
    in the code: neither number appears anywhere in cpmdroid, and
    `copyScreenToClipboard` applies no cap.  Whether one is *needed* is a real
    question and is now an open item in cpmdroid's todo.txt; what is certain is that
    none exists.

<!-- /cites -->

<!-- cites: ioscpm -->
- **Verified iOS/macOS behaviour (2026-09-02), point by point against the spec
  above.** Read against ioscpm at build 58, the commit the `sibling-readings` block
  below records, and confirmed by running the Catalyst build rather than by reading
  alone.
  - **Matches, but only since build 57.** Ring buffer at a single choke point:
    `scrollUp` captures, and `scrollRegion` now delegates to it whenever the region
    is the whole screen, so the line-feed path reaches the buffer.  **Until that
    commit it did not**, which is the entry below.  Mouse wheel at 3 lines/notch and
    **Shift+PageUp/PageDown** / **Ctrl+Home/End** all work; plain PageUp/PageDown
    still reach CP/M through `specialKey`.  Cursor hidden while reading history —
    `ContentView` passes `showCursor: !viewModel.isScrolledBack && viewModel.cursorVisible`.  The view stays
    anchored as output arrives: `scrollUp` advances `scrollbackOffset` by the number
    of lines captured.  Capacity configurable via `scrollbackCapacity` (default
    1000, `0` disables), persisted under the same `scrollbackLines` key this
    document's `display.scrollbackLines` names.
  - **Was broken from build 42 to build 56, in every shipped build and every
    build in between.** The line-feed handler called `scrollRegion` directly, which
    shifts rows and blanks the bottom without keeping what it shifts; only `scrollUp`
    appends to the buffer.  With the default full-screen region — which is nearly
    always — every newline-driven scroll discarded its top line, so the buffer stayed
    empty, `adjustScrollback` clamped every gesture to an offset of 0, and the
    feature was invisible.  The row above was ✅ for six weeks over this.  Two
    further defects would have kept the wheel dead even with a full buffer:
    `handlePan` truncated each notch and discarded the sub-row remainder at `.ended`,
    and its row pitch was `bounds.height / rows` where `draw(_:)` letterboxes by
    `min(scaleX, scaleY)`.
  - **Matches since build 58 (2026-09-02).** History is cleared at both fresh-session
    paths.  It was cleared in `reset()` only, so Stop then Play left the dead session
    above the new banner and could open already parked in history; `resetScrollback()`
    now factors the clear and both `reset()` and `startEmulator()` call it, which is
    the shape and the name this repository already used.  Verified by running it: after
    Stop then Play the status line reads an empty buffer and the previous session's
    output is gone.  So the spec line "cleared on emulator start/reset" now holds on
    two ports of three, cpmdroid diverging deliberately — see its entry above.

<!-- /cites -->

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
- **Verified port behaviour:**
<!-- cites: ioscpm -->
<!-- cites-elsewhere: af0b9b2 -->
<!-- cites-withdrawn: selectionSpan -->
  - **ioscpm (iOS/macOS)** *(re-read 2026-09-06 at `af0b9b2`, build 61 — the shipped 1.5.1)* — ✅ on both now: the Mac
    pointer drag arrived in build 57 and iOS press-and-hold-then-drag in build
    61, which is what this entry used to record as the ◐. Before build 57 there
    was no selection at all - Copy All, ⌘C and ⌘V were all there,
    but `copyText` was the whole-screen copy and there was nothing to select.  A
    pointer drag now runs `handleSelectPan`, which sets an anchor cell and a focus
    cell.  Build 61 moved the state machine both platforms share into
    `driveSelection`, so the Mac's pointer drag and the iOS press-and-drag decide
    the same states once instead of twice: a `TerminalSelection` holds the anchor,
    `visibleSpan` orders it against the focus whichever way the drag went, and
    `isSelected` tests membership.  (Read at build 58 this said `selectionSpan`,
    which is the name build 61 retired.)  The span is **linear** - anchor cell to focus cell,
    wrapping at the row end - rather than rectangular, which is what makes copying
    a wrapped line give you the line.  The highlight is drawn over the cell's own
    background and under its glyph, as a translucent fill rather than this repo's
    inversion, so guest colours stay readable.  `copyText` copies `selectedText`
    when there is a selection and falls back to `copyAllText` when there is not;
    ⌘C and the long-press menu both land there, and `selectedText` trims each
    row's trailing blanks and reads the cells that are on screen, so it copies out
    of scrollback.  Ctrl+C/Ctrl+V are left alone - they are `UIKeyCommand`s on
    Control that fold to `^C`/`^V`.
    **The split is Mac-only, on purpose.** `handlePan` hands a touch-driven drag
    to `handleSelectPan` only under `targetEnvironment(macCatalyst)`; on iOS a
    finger drag is the only way to scroll, so it keeps that job and there is still
    no selection there - the long-press menu offers **Copy All** alone.
    Three gaps against the spec above, on both platforms.  The context menu has no
    Paste - ⌘V is the only way in.  Paste is not gated on the emulator running.
    And the CRLF normalisation is only half done: `pasteText` turns an LF into a
    CR but passes a CR through untouched, so a pasted CRLF reaches CP/M as **two**
    CRs.  Non-ASCII filtering is real - `sendKey` drops any character with no
    `asciiValue`.
<!-- /cites -->
<!-- cites: cpmdroid -->
  - **cpmdroid (Android)** *(2026-09-02)* — ◐, and the "cpmdroid already has this"
    above is the control strip, not selection.  `copyScreenToClipboard` takes the
    whole thing - `historyChars` first, then `screenBuffer`, each row's trailing
    blanks trimmed, no cap - and `pasteFromClipboard` sends the clipboard through
    `sendChar`, dropping anything above 0x7F and mapping LF **and** CR alike to
    0x0D, so a pasted CRLF arrives as two CRs there too.  There is no selection
    model at all: `onTouchEvent` has one meaning for a drag, which is scrollback,
    and nothing anywhere tracks an anchor or paints a highlight.  So the Copy half
    of this row is a Copy All and the drag-select half is absent.
<!-- /cites -->

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
  (`HBF_HOST_GETNAME`, `0xE8`) — **and the shipped 1.0.23 does not deliver one.**
  The package carries no images at all, by design, so `RELEASE_TAG` alone decides
  which combo a user downloads, and at `dbd53b1` it still reads `v1.4.5`, whose
  `hd1k_combo.img` carries neither the host-path usage line nor the `0xE9`
  capability probe. The repin to `v1.4.12` landed after the package was built and
  is delivered by 1.0.24, which is **built and verified at the artifact but never
  submitted**. The read twin is unreachable the same way. So this half of the row
  is true of the tree and of no installed Windows user, which matters because the
  ioscpm cell is docked for the same pin while its shipped build is the one that
  HAS `v1.4.12`. This port also defines
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
<!-- cites: romwbw_emu -->
  - **romwbw_emu (CLI)** *(2026-09-02, at `fce8f87`)* — **both halves, since `98eb6a1`.** `R8`
    copies the command tail at 0x80 and the backend `fopen`s it verbatim
    (`emu_io_cli.cc`, `emu_host_file_open_read`), with a case-insensitive
    per-component retry (`resolve_path_case_insensitive`) to undo the CCP's
:    uppercasing — so any host path works.  `R8` prints the file that was opened
    rather than the path that was typed, the same question `W8` asks about its
    destination: `H_GETRNAME` (`0xEA`) is the read twin of `H_GETNAME`, and the
    CLI's `emu_host_file_get_read_name()` answers with the absolute path it
    resolved.  A directory is refused outright, because `fopen("rb")` succeeds
    on one and only the first read fails, so `R8` used to delete the CP/M file
    of that name and replace it with an empty one.  `W8` is
    `W8 <cpmname> [hostpath]` now::
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
  - **romwbw_emu (web)** *(2026-09-02, `fce8f87`; read from source, never run
    here)* — ✅, and it is a picker and a download rather than a path.  `R8`'s
    string goes to `js_host_file_request_read`, which opens a file picker, and
    only the bytes come back - `emu_host_file_provide_data` takes no name - so
    the CP/M name is still the one `r8.asm` derives from the path that was
    typed, and `emu_host_file_get_read_name()` deliberately answers with an
    empty string so `R8` prints the request rather than pretending to know.
    `W8` buffers the file and `emu_host_file_close_write` downloads it, an
    empty file included; `emu_host_path_basename` cuts a host path down to its
    last component and lowercases it, which is both the download name and what
    `H_GETNAME` answers, so `W8` can print the name the browser will really
    use.  That reduction is also why this backend returns
    `EMU_HOST_CAP_SAFE_PATHS`: there is no directory for a guest path to escape
    into.  Two things the page's own File Transfer panel says about `R8` are
    wrong, and a reader of this row will meet them: the CP/M name does not come
    from the file that was picked, and the substitute for a character CP/M
    cannot hold is `-` rather than the `_` the panel promises - `fcb_char`
    picks it precisely because `_` is a CCP filename delimiter.
<!-- /cites -->
<!-- cites: ioscpm -->
<!-- cites-elsewhere: emu_io_windows.cpp w8.com af0b9b2 -->
  - **ioscpm (iOS/macOS)** *(re-read 2026-09-06 at `af0b9b2`, build 61 — the shipped 1.5.1)* — `W8` always writes `Documents/Exports`, `R8` always reads
    `Documents/Imports` (no per-transfer dialog). As of **v1.4.11 / build 41** an **Import File…** picker (enabled on
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
    **Two changes at `15f48e9`** (2026-08-27), and a re-reading on 2026-09-02
    at build 58 that supersedes half of one of them. `emu_io_ios.mm` defines
    `emu_host_file_get_read_name()`, which that port had to do or stop linking —
    `hbios_dispatch.cc` calls it unconditionally for `HBF_HOST_GETRNAME` and is
    symlinked into `iOSCPM/Core/`, so the core moved under that repo without a
    file there being touched, exactly as `emu_host_path_caps()` did before it.
    At `15f48e9` it returned `""` unconditionally, and this document called that
    the honest answer because the resolved source was known only in Swift.
    **Build 55 stopped that being true.** `emu_host_file_load_named()` carries
    the absolute path the Swift layer really opened down beside the bytes; the
    getter reports it while the state is `HOST_FILE_READING` and `""` at every
    other moment, and the name is cleared on open, close and cancel. It is worth
    having because the CCP uppercases the command line, so a file stored in
    lower case is reached under a shouted name and the delegate's
    case-insensitive scan of `Imports` is what actually finds it. **So this
    row's cross-port contrast is gone**: `emu_io_windows.cpp` records what
    `emu_host_file_open_read()` resolved and opened, that port now does the
    same, and `cpmdroid` closed the identical gap in `167acbe`. What that port
    measured, and wrote into its own source rather than leaving to be found, is
    worth carrying here: with today's `R8` the answer is usually still `""`, and
    correctly so, because `R8` prints its Reading: line between the open and the
    first read, an open there only parks the request, and the guest is rewound
    on `HBF_HOST_READ` until Swift answers — so the state is still
    `WAITING_READ` at the moment the guest asks. And the **zero-byte `W8` export
    bug is fixed** on iOS: `emu_host_file_close_write()` no longer requires a
    non-empty buffer to reach `WRITE_READY`, and `checkHostFileState()` no
    longer guards on the write-data pointer, which by the shared contract is
    `nullptr` for an empty buffer and so could never answer the question. Both
    halves were needed; either alone still swallowed the export. That is the
    same divergence `cpmdroid` closed in `c06fa58` and the browser backend
    closed in v1.36, so all four ports now create the empty file that this one
    and the CLI always created. **The read side of that same hole stayed open**
    until build 55: a `Data` value's base address is nil when it is empty, and
    the hand-off
    sat inside the binding that unwrapped it, so an empty file in `Imports` left
    the backend parked in `WAITING_READ`. This paragraph ended "neither change
    has been compiled — that repo records it as NOT COMPILED, for want of
    Xcode" until 2026-09-02, and that is now false rather than merely stale:
    build 53 was given a build number precisely because the section that had
    been sitting in that repo's CHANGELOG without one had at last been compiled,
    and each of builds 53 to 58 records a clean build — 53 and 56 for the iPhone
    17 Pro simulator and for Mac Catalyst, 54 and 55 for that simulator, 57 and
    58 for Catalyst, those two launched and driven rather than merely built.
<!-- /cites -->
<!-- cites: cpmdroid -->
<!-- cites-elsewhere: c06fa58 -->
<!-- cites-withdrawn: OpenDocument -->
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
    **That paragraph was true until 2026-08-29 and is now the record of what
    changed.** `71465cb` closed it in both directions, and the closure was
    watched on an API 36 emulator rather than argued: a **File transfer**
    screen on the toolbar listing both folders with sizes and dates, with
    **Save as…** (`ACTION_CREATE_DOCUMENT`, behind a small custom
    `ActivityResultContract` because `ActivityResultContracts.CreateDocument`
    fixes the MIME type at registration and each row has a different one),
    **Share** (a `FileProvider` scoped to `Imports/` and `Exports/` and
    deliberately *not* to the external-files root, which also holds the user's
    downloaded disk library), **Delete**, and an **Import** picker
    (`ACTION_OPEN_DOCUMENT`) that mangles the host name to `R8`'s own 8.3 rules
    from `path_to_fcb` in `r8.asm`. The fixed folders did not move and the
    C++ shim was not touched: what was missing was never the boundary, it was
    the UI on top of it. So the weakest cell in this row is now iOS again,
    where the export half is still a fixed folder with no exporter.
    Two capabilities this port has that **this document never had a row to
    ask about**, recorded here because row 4 is where they belong — an import
    is the read half of this row, and both are properties of it:
    **(i) the import is bounded.** `MAX_IMPORT_BYTES` (16 MiB) is enforced in
    `copyBounded()` inside `stageIntoImports()`, which every inbound path funnels
    through, and an over-size source is refused with the partial file deleted
    rather than left as a truncated import that `R8` would happily read. No
    other port caps an import at all; the CLI and the browser hand the whole
    file over. Worth a row of its own only if a second port grows one.
    **(ii) the OS can hand it a file.** `ImportReceiverActivity` carries
    `ACTION_SEND`, `ACTION_SEND_MULTIPLE` and `ACTION_VIEW` intent-filters, so
    **CPMDroid appears in another app's share sheet** — the inbound twin of the
    share this row has always measured on the outbound side. It refuses
    anything but a `content://` Uri, stages through the same 8.3 mangling and
    the same size cap, and reports the mangled name in a toast so the user knows
    what to type at the `R8` prompt. It is the only inbound OS entry point in
    the family that actually does something: iOS has none, the CLI does not need
    one, and this repo *declares* file-type associations in its MSIX manifest
    and then discards the command line — see parity target (c).
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
    behaviour; `ioscpm` closed the same one at `15f48e9`. It was not compiled
    when it was written — no SDK, NDK or Gradle on the machine it was written
    on, and the C++ was built for the host against a stub `jni.h` — and that
    stopped being true on 2026-08-29, on a machine with the SDK, NDK
    28.0.13004108 and an API 36 emulator. `./gradlew clean assembleDebug` built
    the native library for all four ABIs at -Wall -Wextra with no native
    warning, and the fix itself was watched in CP/M 2.2 booted from the combo
    image: `SAVE 0 EMPTY.TXT` then `W8 EMPTY.TXT` prints `Done: 0 bytes` and
    leaves a real zero-byte file in Exports, while `W8 R8.COM` still exports its
    1792 bytes.
    **Re-read on 2026-09-02**, and one thing in this row did move after
    `71465cb`. `167acbe`, later the same day, made
    `emu_host_file_get_read_name()` answer with what was opened rather than with
    what was asked for: the Kotlin layer hands the absolute path of the file it
    resolved down beside the bytes — `provideHostFileData()`, stored by
    `emu_host_set_read_source()` — and the getter is gated on
    `HOST_FILE_READING`, where it used to return the guest's own basenamed
    request at any moment. That port's own note records what the gate is for: a
    bare-FCB `R8` sends no name at all, so the old getter printed a `Reading:`
    line with nothing after it for a file that was being read. `ioscpm` reached
    the same place at build 55, and both ports write down the same measured
    caveat — `R8` asks between the open and the first read, an open on either
    port only parks the request, so the answer at that moment is usually still
    `""` and `R8` falls back to printing what was typed. Nothing else in this row
    moved: the folders, the File transfer screen, the import cap, the inbound
    share target and the C++ containment all read at today's head as described
    above.
<!-- /cites -->
- **Parity targets:** (a) let users reach **arbitrary** host locations within each
  platform's file model — a document picker / `ACTION_CREATE_DOCUMENT`; (b) at
  minimum, **make exports findable**; and (c) — added 2026-08-29, because one
  port now has it and the target list could not describe it — **let the OS hand
  the app a file**, an inbound share/open-with entry point, which is (a) from the
  other side. **(a) and (b) are closed on Android** as of `71465cb` and both
  remain open on iOS; **(c) is closed on Android**, open on iOS, and ➖ for the CLI, where
  `argv` already is the invocation. It is **open on Windows, and worse than
  untouched**: `packaging/msix/AppxManifest.xml` already declares
  `windows.fileTypeAssociation` for `.img`/`.dsk` and `.rom`, so a Store
  install offers this app as an Open-with target — while `main.cpp` takes
  `lpCmdLine` and immediately `UNREFERENCED_PARAMETER`s it, and no window
  accepts `WM_DROPFILES`. The association resolves to a no-op: double-clicking
  a disk image starts the emulator and loads nothing.
<!-- cites: cpmdroid -->
<!-- cites-elsewhere: c26aeb7 c06fa58 -->
  (b) used to say it was "done on Android (share sheet plus the paths shown
  in-app)"; that was the `c26aeb7` reading talking, and it is the last claim of
  that reading left in this row — there is no `ACTION_SEND`, no `FileProvider`,
  no `res/xml` and no path display anywhere in `cpmdroid` at `9b68ab1`, and
  still none at `c06fa58` (re-grepped 2026-08-26) — all four arrived at
  `71465cb` on 2026-08-29, so that sentence is now a statement about two
  particular commits and not about the port. The
<!-- /cites -->
  nearest thing **iOS** has to (b) is `W8` printing its own destination, which
  both ports do; Android went well past (b) at `71465cb`, which is where a File
  transfer screen with sizes, dates, Share and Save as… landed. (a) now needs
  only a document exporter on iOS — the Android half, `ACTION_CREATE_DOCUMENT`,
  is done.
  The shared iOS/Mac **`help_file_transfer.md`** was stale (wrong bundle id
  `com.awohl.iOSCPM`, wrong app name "iOSCPM", no mention of Import File…);
  `ioscpm` commit `9a9d7fd` fixed it on 2026-07-23. `cpmdroid` has its own
  Android-worded copy at `release_assets/help_file_transfer.md`, split off the
  same day (`78e6ec6`), so the two no longer share that text — a change to one no
  longer fixes the other. **That split is now meant to close.** `ioscpm`'s
  `7569745` (2026-08-28) rewrote all eight files in `release_assets/` to stop
  being written for iOS only: Folder Locations now has four platform
  subsections and none of them is the default, one of them this port's, and the
  commit's stated reason is that the Android fork can then go away and all three
  ports read the same file again. As of 2026-09-02 the fork is still there —
  `cpmdroid` still carries its own copy — so until it is deleted the sentence
  above still describes the two trees.

### 5. Remote disk catalog + downloader (pinned)
Download prebuilt disk images from the shared release host instead of bundling
copyrighted content.
- **Behaviour/spec:** fetch `disks.xml`, list catalog (name/desc/status), download
  with progress, track downloaded state, delete. **Pinned to one explicit
  release tag** (not `latest`) so a new release can't silently swap disk images out
  from under an installed client and re-introduce an HBIOS/CBIOS version mismatch.
  *Cancel is in the spec because the ports need it, not because this port has it.*
- **Where:** `z80cpmw/DiskCatalog.{h,cpp}` — note the single `RELEASE_TAG` constant.
- **z80cpmw itself, re-read 2026-09-06 at `dbd53b1`** — the commit the Store's
  1.0.23 was built from, and not at HEAD, where the constant is gone entirely in
  favour of a two-document catalog that has never been built. **Pinned — to
  `v1.4.5`, the OLD pin.** `RELEASE_TAG = L"v1.4.5"` at `DiskCatalog.cpp:17` is
  the single source of both URLs, with the HBIOS/CBIOS-mismatch reason in the
  comment above it. The repin to `v1.4.12` landed in `032b1cf`, **after** the
  packages were built — that commit says so in as many words, "none of this is in
  the 1.0.23 packages … they carry RELEASE_TAG v1.4.5" — and 1.0.24 was cut to
  deliver it and never submitted. So on the installable Windows build this port
  fetches the same `v1.4.5` images cpmdroid is marked down for, and is behind
  ioscpm's shipped build 61 on precisely the axis this row scores. Cancel is the
  same shape as cpmdroid's ◐: it happens when the window dies, not from a button.
- **Shared concern:** all ports download from `ioscpm` releases. **Every port should
  pin to an explicit tag matching the RomWBW version its embedded ROM was built
  from.** See this repo's `WIP`/parity notes on the version-skew problem.
- **Verified port behaviour (2026-08-07):**
<!-- cites: cpmdroid -->
  - **cpmdroid (Android)** *(re-verified 2026-09-02)* — **pinned**.
    `data/DiskCatalogRepository.kt` builds both the catalog URL and the download
    base from a single
    `RELEASE_TAG = "v1.4.5"`, with the reason recorded in a comment (the core's
    HBIOS reports RomWBW v3.5.1, and slices from other releases print an
    HBIOS/CBIOS mismatch). Help deliberately stays on `releases/latest` — see
    item 6 for why that choice is only safe with a bundled fallback.
<!-- /cites -->
<!-- cites: ioscpm -->
<!-- cites-elsewhere: af0b9b2 -->
  - **ioscpm (iOS/macOS)** *(re-read 2026-09-06 at `af0b9b2`, build 61 — the shipped 1.5.1)* — **pinned**,
    since build 42 — but the value is **`v1.4.12`**, repinned by `0010591` in
    build 59 and shipped in 61; this entry read `v1.4.5` when it was taken at
    build 58. `EmulatorViewModel.swift:162` holds the single `releaseTag` from
    which both `catalogURL` and `releaseBaseURL` are built, with the reason in a comment (the core reports RomWBW v3.5.1;
    slices from another release print an HBIOS/CBIOS mismatch). Like cpmdroid,
    help deliberately stays on `releases/latest`, which item 6 explains is only
    safe behind a bundled fallback — and both ports now have one, `ioscpm` since
    build 51 and `cpmdroid` since `1f70c6b`. (This clause read "unlike cpmdroid,
    without a bundled fallback" until 2026-08-29; both halves of that contrast
    are now false.)
    **Three things the downloader gained after that reading**, all at builds 55
    and 56 and all inside what this row's spec calls download and delete. Every
    download is now verified against the catalog's hash before it is installed:
    the check existed but sat in a function whose only callers were its own
    retry arms, so no real download ever entered it, and it now lives in
    `downloadDiskFromSettings`, which hashes the temp file before moving it into
    place - so an installed disk is a passed check. The cached catalog is
    stamped with the pin it was fetched under (`catalogCacheTagKey`), because
    the cache carries one tag's hashes while `parseDiskCatalogXML` always
    rebuilds the URLs from the tag this build is pinned to, so a cache from an
    older pin would pair the wrong hashes with the right URLs; on a mismatch
    `loadCachedCatalog` keeps exactly the entries whose file is already on disk,
    which are never re-downloaded, rather than emptying the catalog on a device
    that has no network to refetch with. And a catalog version bump no longer
    deletes the whole library: `checkCatalogVersionAndInvalidate` now calls
    `deleteCatalogDisks`, which removes only the images the NEW catalog lists,
    so a disk the user imported, a disk `createNewDisk` made, and an image
    dropped from the catalog in the same bump are all kept - nothing can
    re-fetch any of those. The match is case-insensitive because `Documents` is
    published to the Files app on a case-insensitive volume. None of this
    reaches a user yet: that repo records the App Store as serving 1.4.9, builds
    36/37, which predate the pin and all of the above and still fetch from
    `releases/latest`.
<!-- /cites -->
<!-- cites: romwbw_emu -->
<!-- cites-elsewhere: fce8f87 disks.xml -->
  - **romwbw_emu (web)** *(re-read 2026-09-02, at `fce8f87`)* — **there is no catalog
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
    Two smaller facts from the same read. The repository has exactly two disk
    images outside `archive/`, `disks/hd1k_combo.img` and
    `disks/hd1k_infocom.img`; of the five names the
    page offers, four exist nowhere in the tree, and the one image it does have
    besides the combo is not offered. And the ROM select has the same shape but
    was already fixed on the packaged path only: it offers one ROM,
    `emu_avw.rom`, fetched the same bare relative way, and the release workflow
    stages it beside the page with a comment recording exactly that lesson —
    while the two makefile deploy targets, which nothing checks, still do not
    copy it. `web/emu_romwbw.rom` is tracked and referenced by nothing.
    **The missing image and the duplicated ROM are a ruling nobody has made,
    not work nobody has noticed**, and since `41565a1` the tree says so:
    `DECISIONS.md` carries "What a package ships: the duplicated ROM, and the
    missing disk" as one item, and states the same two facts this row does -
    that no `.img` is staged by `release.yml` or by `web/makefile`'s deploy
    targets, and that `z80cpm_tools.img` exists nowhere in this tree.  Shipping
    the 49 MB combo image in a deb is what the answer costs, which is why it is
    a question rather than a chore.

<!-- /cites -->
### 6. Remote help system + bundled fallback
In-app help fetched from GitHub, with offline bundled topics.
- **Behaviour/spec:** `help_index.json` + markdown topics downloaded and cached;
  bundled "Getting Started" and "Configuration" topics always available offline; a
  small markdown→text renderer (headers, tables, lists, inline code).
- **Where:** `z80cpmw/HelpWindow.{h,cpp}` and `HelpAssets.{h,cpp}` — the
  state-free half (index parsing, the markdown→text renderer, and the cache)
  was split out on 2026-08-28 so it could be put under test, and is **353**
  checks in the shipped 1.0.23 — 244 was the figure before the bundled-asset
  section existed, and it stood here until 2026-09-06.
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
<!-- cites: cpmdroid -->
<!-- cites-elsewhere: aee7276 build.gradle.kts -->
- **Verified port behaviour (2026-08-29, superseding the 2026-08-25 entry):**
  **cpmdroid is out of the trap**, and the 2026-08-25 entry — which correctly
  found nothing bundled, no cache, and a `build.gradle.kts` that mentions help
  nowhere — is now the record of what was fixed rather than a live finding.
  `1f70c6b` and `aee7276` closed it. All eight files (`help_index.json` plus
  seven topics) ship inside the APK under `app/src/main/assets/help/`,
  byte-identical to the copies in `release_assets/` that a release is meant to
  carry, so there is no second copy to drift; no Gradle rule was needed
  because AGP packages `src/main/assets/**` verbatim. **Nothing is attached
  today, which is the trap seen from the other side.** The releases were listed
  on 2026-09-02: `v1.14`, which is what `releases/latest` resolves to, carries
  `app-release.apk` and nothing else; help assets were last attached to `v1.11`
  and only two of the eight; `v1.0` is the one release that ever had all eight.
  So `INDEX_URL` returns 404 for every reader, every time — exactly what the
  comment above it predicts — and the bundled copy is doing the whole job. That
  is not a defect in this port's fallback, it is what the fallback was built
  for, but it does mean the download half has no live asset to be right about. The order is **download,
  then the on-disk cache, then the shipped copy** — `resolveHelpIndex` and
  `resolveContent`, with a `HelpSource` enum that puts "offline copy, saved
  <date>" or "bundled with the app" in the action-bar subtitle so a reader can
  tell which one they got. The cache is under `filesDir`, deliberately not
  `cacheDir`, "because the whole point is a topic that is still there on a train
  weeks later". It still **prefers** the published index, which is the right way
  round: a correction reaches users without an app update.
  Two details worth taking rather than re-deriving. A blank or zero-byte cached
  file is treated as a **miss**, not as an empty topic, because "a blank pane
  cannot be told apart from a topic that loaded and had nothing to say"; and a
  truncated download never becomes a cache entry, because the fetch compares the
  body's byte count against a declared `Content-Length` and rejects a mismatch.
  Three of the seven topics were also reworded from iOS to Android on the way
  (`aee7276`) — they had been describing an iPhone to an Android reader.
<!-- /cites -->
<!-- cites: ioscpm -->
- **Verified ioscpm behaviour (2026-09-02, build 58):** help **yes**, fallback
  **yes, since build 51** — the port this trap applied to on 2026-08-24 is out of
  it, and as of 2026-08-29 so is `cpmdroid`, which leaves **no GUI port in it**. `HelpView.swift` still resolves
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
  The consequence of preferring the published copy that this row used to carry
  is **closed, and measured rather than inferred**.
  `release_assets/help_quick_start.md` was corrected in build 49 — it advertised
  a `Ctrl+E` emulator console that does not exist, which is actively harmful
  because `^E` is WordStar cursor-up — and a networked user kept reading the
  stale published text until the file was re-attached. Build 56 (2026-09-01)
  attached all eight, `help_index.json` and the seven topics, to `v1.4.11`,
  which is what `releases/latest` still resolves to; all eight were fetched on
  2026-09-02 and every one is byte-identical to `release_assets/`, and
  `ioscpm`'s `todo.txt` no longer carries the chore. Two things from that
  attempt are worth taking rather than re-deriving: the first pass uploaded two
  files from a stale checkout and left the published set at two vintages, and
  `releases/latest/download/` kept serving the old bytes through a CDN for
  minutes afterwards, so the check has to go through the tagged release rather
  than the redirect.

<!-- /cites -->
### 7. NVRAM / autoboot / boot string
- **Behaviour/spec:** RomWBW autoboot config via `W` at the boot menu persists
  and "Clear Boot Config" resets it. **The setting travels in the emulated RTC's
  NVRAM switches; nothing is typed at the boot menu.** `EmulatorEngine::start`
  calls `setNvramSetting(m_bootString)` under the comment "Configure boot option
  via NVRAM switches (not character queueing)", and `sendString` — the only
  character-queueing entry point on the engine — has no caller anywhere in
  `z80cpmw/`, only its definition and its declaration.
  This row said "an optional `bootString` is auto-typed at the boot menu" until
  2026-09-06, and that described **1.0.7**: `eb97c64` ("v1.0.8: Fix boot option
  using NVRAM switches") deleted the `emu_console_queue_char` loop as broken on
  2026-01-08, fifteen releases before the build the Store serves. **No port has
  the auto-type, this one included**, so the ⬜ it was creating in the other
  three columns was scored against software that has not existed since 1.0.8.
  **Note the boot-unit numbering:** with the EMU AVW ROM the on-board RAM/ROM
  disks are units 0 and 1, so the first hard disk is unit **2** — see this repo's
  Getting Started help for the user-facing wording.
- **Where:** `z80cpmw/EmulatorEngine.cpp` (`clearNvramSetting`) and
  `z80cpmw/EmulatorEngine.h` (`setBootString`, inline),
  config `core.bootString`. (ioscpm already has NVRAM boot config.)
<!-- cites: cpmdroid -->
<!-- cites-withdrawn: bootString -->
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

<!-- /cites -->
<!-- cites: ioscpm -->
- **Verified ioscpm behaviour (2026-09-02, build 58) — the first one written
  down for that port in this row, and the flat ✅ in the matrix was wrong:**
  NVRAM persistence is **present**; the **`bootString` auto-type is not**, which
  puts this port exactly where Android is. `bootString` in
  `EmulatorViewModel.swift` is that port's name for the NVRAM setting rather
  than a host-typed string: its `didSet` writes it to `UserDefaults` under
  `nvramKey`, `loadSelectedResources` (reached from `startEmulator`) and `reset`
  both push it into the guest with `setNvramSetting`, and `emulatorVDAWriteChar`
  polls `hasNvramChange` on every character written and calls
  `syncNvramFromEmulator`, so a `SYSCONF` edit made inside the guest comes back
  out to the UI and to `UserDefaults`. `ContentView.swift`'s Boot Options
  section shows it read-only, captions it "Configure via ROM 'W' menu
  (SYSCONF)", and offers **Clear Auto-Boot** (`clearAutoboot`) — this row's
  "Clear Boot Config". The auto-typed string is a different thing and nothing
  reaches it: `setBootString` is in the vendored core and on the Objective-C
  bridge, no Swift file names it, and the only call that runs is
  `setNvramSetting`'s own guard clearing it to the empty string. Two loose ends
  found while reading, neither of them a parity claim: `loadNvram` is private
  and has no callers, and `isNvramInitialized` is on the bridge and is never
  called from Swift, where Android does call it.

<!-- /cites -->
### 8. Desktop window state (Windows/macOS only)
- **Behaviour/spec:** remember main-window position/size across runs with
  monitor-change / off-screen reset; auto-size the window to the exact 80×25 grid on
  font change; per-monitor DPI-v2 font scaling.
- **Where:** `z80cpmw/MainWindow.cpp` (`WindowConfig`, `restoreWindowPlacement`,
  `saveWindowPlacement`, `resizeWindowToTerminal`), `TerminalView::createFont`;
  config `window` block. **N/A to mobile** — but not to Mac Catalyst, which is a
  resizable desktop window.
- **Verified z80cpmw behaviour (2026-09-06, at `dbd53b1` — the 1.0.23 build):**
  all three clauses of the spec ship. Placement: `saveWindowPlacement` writes the
  rectangle, the maximized flag and the monitor bounds into the config `window`
  block on close and on end-of-session; `restoreWindowPlacement`, called from
  `MainWindow::show` ahead of the `resizeWindowToTerminal` fallback, discards a
  saved rectangle no monitor covers. Auto-size: `resizeWindowToTerminal` runs
  after every View-menu font change and expands the frame with
  `AdjustWindowRectExForDpi`. DPI: the app really is per-monitor v2 —
  `z80cpmw.manifest` declares `PerMonitorV2`, and `TerminalView::createFont`
  scales the height by `GetDpiForWindow` through `MulDiv(m_fontSize, dpi, 96)`.
  The one gap is narrower than "no DPI support": there is **no `WM_DPICHANGED`
  handler**, so the font is scaled when it is created and dragging the window to
  a monitor at a different scale does not re-create it. That distinction matters
  outside this row — the ioscpm cell is docked for having no per-monitor DPI
  scaling, and that dock is correct precisely because the Windows reference does
  have it.
<!-- cites: ioscpm -->
<!-- cites-elsewhere: af0b9b2 -->
<!-- cites-withdrawn: NSUserActivity -->
- **ioscpm (iOS/macOS)** *(re-read 2026-09-06 at `af0b9b2`, build 61 — the shipped 1.5.1)* — ◐ on Catalyst, no
  longer absent. `8e7587f` added `WindowFrame` and `CatalystWindow`: four numbers
  under a `catalystWindowFrame` default, saved when the scene deactivates and
  restored a runloop turn after `onAppear`, with a restored frame clamped to the
  display, an off-screen one dragged back on, and a 640×480 floor held through
  `sizeRestrictions`. Placement needs iOS 16's `requestGeometryUpdate` and
  `IPHONEOS_DEPLOYMENT_TARGET` is 15.0, so below that only the minimum applies.
  Tested only where it can be: `WindowFrame` has 34 checks, `CatalystWindow` has
  none, and MANUAL_CHECKS §10's window boxes are unticked — it has never been
  driven on a Mac. Font size is still a menu rather than a grid-derived size, so
  there is no auto-size-to-80×25, and there is no per-monitor DPI scaling.
  *(Superseding the 2026-08-24 reading, which found no stored frame at all.)*

<!-- /cites -->
### 9. Configurable font size
- **Where:** config `display.fontSize`, View menu (`MainWindow::onViewFontSize`).
  All GUI ports should expose this; mobile typically pinch-to-zoom.
<!-- cites: cpmdroid -->
<!-- cites-elsewhere: Int.MAX_VALUE -->
- **Verified Android behaviour - re-read 2026-09-02:** **present** - a slider in
  Settings (`fontSizeSeekBar`, shown in pt) stored as the `font_size` preference,
  applied through `TerminalView.customFontSize`.  There is **no** pinch-to-zoom.
  The 8–24 range is real, but it is enforced **only on the slider**, which is the
  opposite of what the 2026-08-07 reading said: `android:min` and `android:max`
  sit on the seek bar in the settings layout and nothing behind them clamps.
  `getSettings` reads `font_size` back raw; `saveSettings` and `setFontSize`
  write it through unchecked; the settings screen hands `saveSettings` the seek
  bar's `progress` directly; and `scrollbackLines` is the only field
  `SettingsRepository` coerces at all.  That matters because `android:min` on a
  `SeekBar` needs API 26 and is ignored on 24–25, and this app's `minSdk` is 24,
  so on those two levels the slider reaches 0 - and a zero font size leaves
  `calculateFontSize` dividing the available width by a zero `charWidth`, which
  saturates the column count to `Int.MAX_VALUE` and kills the app on every launch
  once the value has been persisted.  That last step is read from the source, not
  watched running on an API 24 device.  So the lesson is worth copying and the
  code is not: clamp where the value is *stored*, not where it is *entered*.
<!-- /cites -->
<!-- cites: ioscpm -->
- **Verified ioscpm behaviour (2026-08-24):** **present**, as a six-step menu
  (14, 16, 18, 20, 24, 28 pt) in `ContentView.swift` — the `Label("Font Size…`
  menu — persisted with `@AppStorage("terminalFontSize")` and applied by
  recreating the terminal view
  on change. A fixed choice list rather than a slider, so Android's stored-value
  clamp problem cannot arise here. There is no pinch-to-zoom.

<!-- /cites -->
### 10. Cromemco Dazzler graphics card (optional)
Emulated retro graphics card in a separate window.
<!-- cites: romwbw_emu -->
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
  (`src/emu_io_wasm.cc`) and implemented nowhere.  Not quite nowhere: the
  `js_error` shim has fallen back to `console.error` when the page defines no
  handler since the very first commit, so the errors reached a devtools console
  nobody had open and never the page itself.  The page implements it now, to
  the status line and to `console.error` both, which is a large part of why the
  dead wiring above survived unnoticed for so long.
<!-- /cites -->
- **Behaviour/spec:** enable + base I/O port + scale, rendered in its own window.
- **Where:** `z80cpmw/Dazzler.cpp`, `DazzlerWindow.cpp`; config `hardware.dazzler`.
<!-- cites: cpmdroid -->
- **Status:** absent in iOS. **Android is not partial — it is stubbed out on
  purpose** (verified 2026-08-07): `app/src/main/cpp/emu_io_android.cpp` defines
  `dazzler_port_in` returning 0 and `dazzler_port_out` doing nothing, both marked
  "stubs - not used on Android", next to the same treatment of the DSKY. Nothing
  in the Kotlin layer mentions the Dazzler, and there is no window, no setting
  and no rendering. Treat Android as ⬜, not ◐: a guest can write to the ports
  without an error, and nothing whatever happens. Low priority unless a port
  specifically wants it.

<!-- /cites -->
### 11. Config profiles & JSON config
- **Behaviour/spec:** named config profiles (save/load/delete); single JSON config
  file with migration from the legacy INI format.
- **Where:** `z80cpmw/Config.{h,cpp}` (`ConfigManager`). Each port keeps its own
  config format; parity is the *set of settings*, not the file format.
<!-- cites: cpmdroid -->
<!-- cites-elsewhere: romwbw_emu -->
<!-- cites-withdrawn: saveProfile loadProfile listProfiles deleteProfile -->
- **cpmdroid (Android)** — ⬜. There are no profiles: `saveProfile`,
  `loadProfile`, `listProfiles` and `deleteProfile` do not exist, and there is
  no Settings → Configuration Profiles. `SettingsRepository` is a flat
  `SharedPreferences` wrapper over one current setting each (ROM, four disk
  slots, font size, wrap, scrollback, sound, manifest warning, NVRAM). That is
  the same shape as the `romwbw_emu` CLI's single settings file.

<!-- /cites -->
<!-- cites: ioscpm -->
<!-- cites-elsewhere: af0b9b2 -->
  - **ioscpm (iOS/macOS)** *(re-read 2026-09-06 at `af0b9b2`, build 61 — the shipped 1.5.1)* — ✅, and this row was
    ⬜ for it until the re-read. `ProfileSection` in Settings is ungated, so iOS
    and Catalyst both have it: named `EmulatorProfile`s carrying ROM, four disk
    slots, boot string, key profile with its custom bindings, scrollback
    capacity, bell, manifest warning, key row and new-disk size, with save,
    tap-to-apply, swipe-to-delete and update-in-place, the whole `ProfileStore`
    persisted as one JSON value under the `emulatorProfiles` default, and 66
    checks in `EmulatorProfileTests`. It landed in `8e7587f`, which is still
    stamped build 58 — the same number as the commit the previous reading was
    taken at — which is how it went unrecorded. Font size is the one setting a
    profile does not carry, living alone in `@AppStorage("terminalFontSize")`;
    file-backed local disks are stored empty on purpose, a bookmark being a token
    and not a name, at the cost that applying a profile can never detach one; and
    `renameProfile` exists with no UI reaching it. So this row is ✅ on two of the
    four ports, not one.
<!-- /cites -->
### 12. Manifest-disk write warning
- **Behaviour/spec:** warn before writing to a downloaded catalog ("manifest") disk,
  since a re-download would overwrite local changes. Suppressible.
- **Where:** config `core.warnManifestWrites` (default `true`); the *Warn on
  Downloaded Disk Writes* checkbox on `SettingsDialogWx.cpp`'s Disk Images page;
  `EmulatorEngine::setDiskIsManifest`, `setDiskWarningSuppressed` and
  `pollManifestWriteWarning`, which forward to the shared core this project
  compiles directly — the same `hbios_dispatch.cc` the two mobile ports build.
- **Verified z80cpmw behaviour (2026-09-06, at `dbd53b1` — the 1.0.23 build, and
  unchanged at HEAD):** **present and suppressible.** `MainWindow.cpp`'s 500 ms
  status tick polls `pollManifestWriteWarning()` and raises a *Disk Write
  Warning* box pointing at File → Save Disk; like ioscpm's it is informational,
  the write having already happened, and it fires **once per session** for the
  same reason as on the other two ports — the latch is a static in that shared
  core file and the core's `reset()` leaves it alone. Settings OK pushes the
  checkbox both ways across all four units, so re-ticking clears the
  suppression. No column is over-scored by this row: ioscpm and cpmdroid are
  both already ✅ here, so correcting the reference inflates nothing.
<!-- cites: cpmdroid -->
- **Verified Android behaviour (2026-09-02):** **present and suppressible.**
  Downloaded (catalog) disks are flagged per unit through
  `EmulatorEngine.setDiskIsManifest` as they are mounted; the emulation loop
  polls `checkManifestWriteWarning` and raises a dialog **once per session**;
  Settings has a *Warn when writing to downloaded disks* checkbox
  (`SettingsRepository.isWarnManifestWritesEnabled`, default **true**), and the
  preference migration deliberately drops the pre-v3 stored `false` so the newer
  default takes effect. One asymmetry, if you copy the code: turning the
  checkbox **off** pushes `setDiskWarningSuppressed(unit, true)` to all 16 units
  (`applyManifestWarningPreference`), but turning it back **on** does not clear
  the suppression — that waits for the next disk reload or boot.
<!-- /cites -->
<!-- cites: ioscpm -->
- **Verified ioscpm behaviour (2026-09-02, build 58):** **present and
  suppressible.** A "Disk May Be Overwritten" alert (grep `ContentView.swift`
  for that string — it has two presentation sites) fires on a write to a catalog
  disk, and Settings has a *Warn on Downloaded Disk Writes* toggle bound to
  `EmulatorViewModel.warnManifestWrites`, persisted in `UserDefaults` with the
  default applied when the key is absent. The alert is informational — it points
  the user at *Save Disk As* rather than offering to cancel the write. It fires
  **once per session**, exactly as the Android one does and for the same reason:
  the two ports compile the same core file — this port's
  `iOSCPM/Core/hbios_dispatch.cc` is a symlink into `romwbw_emu` — and the flag
  `pollManifestWriteWarning` sets is a static that `reset()` deliberately leaves
  alone. Two asymmetries against Android, in opposite directions: turning the
  toggle back **on** here does clear the suppression, because
  `applyWarningSuppression` pushes the current setting either way — but it
  pushes it to units 0-3 only, where Android walks all 16.

<!-- /cites -->
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
  per-cell foreground **and background** so reverse video renders; per-cell
  bold, underline and blink since its build 55, with the bright SGR halves
  90-97 and 100-107 and the defaults 39 and 49; TAB advancing to the next
  8-column stop.
- **Where (per port):**
<!-- cites: ioscpm -->
<!-- cites-elsewhere: cpmdroid TerminalView.kt c0b3bf7 af0b9b2 -->
  - **ioscpm** *(re-read 2026-09-06 at `af0b9b2`, build 61 — the shipped 1.5.1)* — `iOSCPM/Views/TerminalScreen.swift` since `8e7587f`, with the whole of SGR now in `TerminalRendition.swift`.
    The origin of the parser: full VT52, scrolling region, answerbacks, deferred
    autowrap, charset consumption. **Build 51 closed the gap this entry used to
    name.** `@` (ICH), `P` (DCH), `X` (ECH), `S` (SU) and `T` (SD) are all
    implemented, and DECAWM (`?7`) and DECTCEM (`?25`) are acted on rather than
    parsed and dropped — both returning to their power-on state on cold boot, so
    a guest that hides the cursor and dies does not leave it hidden for the next
    session. SU routes through `scrollUp()` only when the region is the whole
    screen and through `scrollRegion()` otherwise, so lines pushed out of a
    status-line window are never history. **LF did not follow that rule until
    build 57**, which is what this entry used to claim it did: the LF handler
    called `scrollRegion()` unconditionally, and that function did not capture,
    so no ordinary newline ever reached the buffer - see row 2. The whole-screen
    test now lives inside `scrollRegion()`, so both paths get it.
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
    to.
    That reading turned up a different difference, and it did not survive the
    week: **`applySGR` there had no bright half and no per-cell face.** The
    whole switch was `0`, `1`, `22`, `7`, `27`, `30...37`, `40...47` and
    `default: break`, so `ESC[91m` left the attribute byte alone and the text
    drew in whatever was current - exactly what this repo did until `978b623`.
    `cpmdroid`'s `TerminalView.kt` had carried the bright half since its own
    ANSI fix, and `ioscpm` was briefly the one port without it. **Build 55
    closed both halves on 2026-09-01**, and moved the whole of SGR out of the
    view model into `TerminalRendition.swift`: `90...97` set the colour with the
    intensity bit and `100...107` fold onto the plain background, for the same
    three-bit reason this repo folds them; `CellFlags` carries bold, underline
    and blink with this repo's three values byte for byte; and
    `TerminalView.swift` paints them, through a bold `UIFont`, an underline
    attribute and a 500 ms blink phase on the cursor's own timer. An erase there
    zeroes the flags and keeps the colours, as here - `blankCell` is built from
    `displayAttr`. `TerminalRenditionTests.swift` drives all of it headlessly,
    which is the first unit test any part of that parser has had. `todo.txt`
    carries neither item now; what it carries is that the faces have been
    watched on the simulator and not yet on a device.
    Parser input **is** bounded, since build 49: `maxCSIParams` 16 and
    `maxCSIParamDigits` 6, matching z80cpmw (`cpmdroid` had none until
    `c0b3bf7`, 2026-08-29, and arrived last of the three), with leading zeros dropped so
    zero-padding cannot spend the digit budget. Build 49 also made SGR 7 a
    render-time toggle instead of an in-place nibble swap, so SGR 27 restores
    the original colours instead of resetting to white-on-black.
<!-- /cites -->
<!-- cites: cpmdroid -->
<!-- cites-elsewhere: ioscpm HFONT TCELL_BOLD TCELL_UNDERLINE TCELL_BLINK kotlinc c6756af c0b3bf7 -->
  - **cpmdroid** *(2026-08-29 at `167acbe`, re-read 2026-09-02 at the current
    tip, where nothing in this row has moved — see the caveat
    at the end of this bullet)* — ✅, and this row has moved further in one day
    than any other cell in this document. It was the row where the 2026-08-07
    reading was furthest from the code, and the ⬜ that replaced that reading
    was accurate until 2026-08-29.
    **What that ⬜ recorded**, kept here because it is what the closure has to
    be measured against: no VT52, no DECSTBM, no DECSC/DECRC, no answerbacks,
    no private-mode handling and no `P @ X S T`; a CSI dispatch of `H f A B C D
    J K m` and nothing else; foreground-only SGR; and ESC followed by anything
    but `[` discarded, which is what made every sequence above unreachable
    rather than merely unimplemented. Every item in that list was accurate at
    `c6756af`, including "no parameter bounds" — the bounds are `c0b3bf7`
    (2026-08-29), part of the round this bullet records, and before it the
    parameter buffer was an unbounded `StringBuilder` that an unterminated
    escape could grow without limit.
    **What is there now.** `processEscape()` is a dispatch table, so the ESC
    branch no longer discards: `7`/`8`, `D`/`E`/`M`, `c` (RIS), `Z`, `<`,
    `=`/`>`, and the designators `( ) * + #` and space consumed with their
    argument byte. The full VT52 set with the same auto-detection this repo
    uses — receiving a VT52-exclusive escape is itself the signal — and `ESC Y`
    taken through two dedicated parser states. DECSTBM with a region-aware line
    feed, `IL`/`DL`/`SU`/`SD` and `RI` honouring it. DECSC/DECRC saving the
    rendition as well as the position, and `CSI s`/`u` sharing the slot. The
    seven editing finals — `@ P X L M` and the two scrolls `S T` — and
    `G`/`` ` ``/`d`. Parameter bounds now match this repo and `ioscpm`:
    `MAX_CSI_PARAMS` 16, `MAX_CSI_PARAM_DIGITS` 6, values clamped to 9999. The query
    replies (`ESC Z`, `CSI c`, `CSI 5 n`, `CSI 6 n`) with the private forms
    deliberately silent. DECANM, DECAWM and DECTCEM acted on, with the private
    marker remembered rather than merely swallowed. Deferred autowrap.
    **Per-cell attributes**, with the same three bits and the same values as
    this repo's `TCELL_BOLD`/`TCELL_UNDERLINE`/`TCELL_BLINK`, painted through
    four `Paint` objects indexed by the flags — the same shape as the four
    `HFONT`s here and adopted for the same reason.
    Three divergences that are choices, not gaps, and are marked as such in
    that port's own `todo.txt` so a later sweep stops re-filing them: **SGR 0
    resets the foreground to green**, not CGA 7, because that is the app's
    identity; **`ESC[104m` stays bright**, because a cell there is a full ARGB
    `Int` with no blink bit to borrow and so nothing forces the 100–107 fold
    this repo makes; and **SGR 1 does not brighten the colour**, because bold
    and bright are only the same thing inside a packed attribute byte. That
    port also implements **SGR 39 and 49**, and so does `ioscpm` since its
    build 55 - which leaves this repo the only one of the three without them.
    One thing it still does not have that the others do: it prints every byte
    from 0x20 **up**, where both other ports stop at 0x7E, so DEL and the whole
    0x80–0xFF range draw glyphs here and nothing there. That one is in its
    `todo.txt`. FF is still discarded — but so it is by both other ports, so it
    belongs on the list of things none of the three has rather than in this
    cell.
    **The caveat.** `167acbe` was written on a machine with no Android SDK and
    **has never been run**. It is compiled — the C++ by host `clang++` at `-Wall -Wextra`, and
    `TerminalView.kt` by `kotlinc` against real Android framework classes — and
    that is a type-check, not a screen. Its `MANUAL_CHECKS.md` gained a section
    listing what to point at it. Read this ✅ as "the code is there and agrees
    with this document", not as "somebody watched it work"; the other three
    columns in this row do not carry that qualifier.
    On the history: the mobile ports did not jointly lead this row. **`ioscpm`
    did**, and z80cpmw's item 13 work came from there.
<!-- /cites -->
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
    `tests\run_tests.bat` runs it first of **six suites, 1323 checks** — the
    figure for `dbd53b1`, the 1.0.23 the Store serves, and the one that build's
    own CHANGELOG entry records. The **seven suites, 1467 checks** that stood
    here is 1.0.25's: its seventh suite is disk provenance
    (`tests/test_diskledger.cpp`), added after the shipped build, and 1467
    appears nowhere in the 1.0.23 tree. Every capability this row claims IS in
    the shipped build; only the evidence figure was borrowed from an unbuilt
    version. The
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
    deliberately still preserves the scrolling region, and `ioscpm` now agrees:
    `ESC [ 2 J` there routes through its own `eraseScreen()`, and only the
    machine-level `clearTerminal()` resets the region. That was `ioscpm`'s bug
    when this paragraph was written; it is the same split as here now.

    **Also fixed 2026-08-28:** SGR **90–97** and **100–107** were not handled at
    all — they fell through `applySGR()`'s default and left the attribute byte
    alone, so `ESC[91m` from a fresh reset drew in CGA 7. Measured by
    `tests/test_render.cpp` on its first run, not inferred. The bright half is
    the same ANSI index with the intensity bit set, which is the bit SGR 1
    already sets, so `22` dims a bright colour and a `3x` after a `9x`
    deliberately does not; `100–107` fold onto the plain background, because the
    background nibble is three bits and the fourth is blink on real hardware,
    and a wrong shade beats a cell that starts strobing.
<!-- cites: romwbw_emu -->
  - **romwbw_emu** *(re-verified 2026-08-24)* — the CLI delegates to the host
    terminal, but not transparently: `emu_console_write_char` in
    `emu_io_cli.cc` does `ch &= 0x7F` and then drops every CR, not
    just the CR of a CR LF pair — so a guest returning to column 0 without a
    newline (progress counters, status-line redraws) overwrites nothing, and
    8-bit output is gone before the tty sees it.

    The web build loads **xterm.js 5.3**, vendored under `web/vendor/` since
    `5920681` rather than fetched from a CDN, and it *is* a far more complete VT
    than any native front end; since `2dbf6f2` the page no longer starves it.
    `Module.onConsoleOutput` (the `Module.onConsoleOutput =` assignment in
    `web/romwbw.html-template`) hands every byte to `term.write()` unchanged,
    with LF the single exception: it is written as CR LF, because a CP/M guest's
    bare LF means new line and xterm.js would otherwise leave the column where
    it was. The filter it replaced passed only CR, LF, BS, ESC and `0x20–0x7E`,
    dropped **TAB, BEL, FF, every other control byte and everything ≥ 0x7F**,
    and rewrote BS as `\b \b` — a *destructive* backspace, so a guest moving the
    cursor left erased a character. CSI sequences survived that only because
    their bodies happen to be printable ASCII. The row was ✅ on the strength of
    the library while the wiring was what decided it; the page now agrees with
    the library. **The backend does not**, and it is the CLI's own defect one
    layer down: `emu_console_write_char` in `src/emu_io_wasm.cc` does `ch &= 0x7F`
    and drops every CR before the page sees a byte, so a guest returning to
    column 0 without a newline overwrites nothing here either, and an 8-bit byte
    arrives as its low seven bits. That is why this cell is ◐ and not ✅: what
    `2dbf6f2` fixed is the page half of a two-layer filter.
    `tests/web_console_output.js` walks all 256 byte values through the handler
    lifted out of the template, which proves the page hands every byte on and
    proves nothing about the stream reaching it. Read from source only - node is
    absent from this machine, so neither web test was run here, and no browser
    has drawn the page at all since xterm was vendored.
<!-- /cites -->
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

Android `cpmdroid` **was re-read in full on 2026-09-06, at `35873d0` —
versionCode 27, the bundle Google Play actually serves as 1.25.** Which commit
that is comes from cpmdroid's own `a24ca9a`, whose message says "The uploaded
bundle is 35873d0"; the two other commits sitting at versionCode 27 change no
app source. Four of the thirteen rows moved, and unlike the ioscpm column they
did not all move one way: rows 2 and 13 understated what shipped, while **rows 1
and 5 credited this port with more than a Play user has** — row 1 hung F1–F12,
the nav cluster and Ctrl+arrow all on `c0b3bf7` when two of the three predate it
in `a523d40`, and row 5 was a flat ✅ for a downloader with no delete and no
cancel. This supersedes the 2026-08-25 reading of the whole column (`9b68ab1`,
made after the `c26aeb7` citations turned out to describe code that was never
pushed), its 2026-08-27 re-verification at `c6756af`, and the 2026-08-29 re-read
of rows 4, 6 and 13. See the note at the head of this file.
The **iOS/macOS column was re-read from `ioscpm` source on 2026-09-06**, at
**`af0b9b2`, build 61** — deliberately not at HEAD, because build 61 is the
commit the App Store's 1.5.1 was built from and builds 62-65 have never been
compiled. Every row was re-derived. **Seven of the thirteen changed and all
seven in the same direction: the column understated what ships.** Six of those
had gone unrecorded because the work landed in `8e7587f`, which is still stamped
build 58 — the same number as `e33beea`, the commit the previous reading was
taken at, so nothing about the version numbers said the tree had moved. Row 5
was the exception and was simply wrong: it said the pin was `v1.4.5` when build
59 had repinned to `v1.4.12`. Nothing in the column credited ioscpm with
anything absent from the shipped build, which is the error this file exists to
catch; the whole of it ran the other way.
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
z80cpmw    dbd53b1  2026-09-06  shipped:1.0.23
ioscpm     af0b9b2  2026-09-06  shipped:61
cpmdroid   35873d0  2026-09-06  shipped:27
romwbw_emu fce8f87  2026-09-02  shipped:1.38
```

What each of those three readings is, because they are not the same kind of
thing:

- **`ioscpm` `af0b9b2`** - a full re-read of all thirteen rows on 2026-09-06,
  and the first one taken at a SHIPPED commit rather than at whatever the tree
  happened to be. That is the point: `af0b9b2` is build 61, which is the 1.5.1
  the App Store serves, so every tick in this column now describes software a
  user can install. The tip is no longer frozen for effect - the previous line
  held `15f48e9` on purpose so the drift script would keep reporting the column
  as moved, and the earlier readings it stacked on (`49851aa` at build 52,
  2026-08-26; `0165dac` and `0dbab43` read for row 13 alone on 2026-08-28) are
  superseded by this one. This is also the only column whose shipped build is
  measured rather than asserted: `ioscpm/tools/check-store-version.sh` queries
  the iTunes lookup, and it reports "at most build 61" because builds 62-65 each
  carry a `**NOT COMPILED` marker.
- **`z80cpmw` `dbd53b1`** - this line used to read `HEAD`, and that was the
  document's oldest unexamined assumption: the reference column was "maintained
  in place", edited in the same commit as the code it describes, so it always
  described the TREE. The tree is not what anyone runs. All thirteen rows were
  re-read on 2026-09-06 at `dbd53b1`, the commit the Store's 1.0.23 was built
  from - not at HEAD (1.0.25, never built) and not at the newest commit numbered
  1.0.23 (`032b1cf` changed five source files after the package was already made
  from `bin\Release`). Eight rows moved. From here this column carries a reading
  like the other three and has to be re-read when the Store moves, rather than
  drifting forward with the tree. Its shipped figure is still the weakest of the
  four: nothing in this family queries the Microsoft Store, so `shipped:1.0.23`
  is this repository's own CHANGELOG assertion, not a measurement.
- **`cpmdroid` `35873d0`** - a full re-read of all thirteen rows on 2026-09-06,
  taken at the SHIPPED bundle rather than at a tree: versionCode 27 is what Play
  serves, and `a24ca9a` records that 35873d0 is what was uploaded. It stacks on
  the 2026-08-25 full reading (`9b68ab1`), its re-verification at `c6756af`
  (documentation only; `c06fa58` was the source change, in row 4) and the
  2026-08-29 re-read of rows 4, 6 and 13, and supersedes all three. Unlike
  ioscpm's, this column was wrong in BOTH directions - see the snapshot above.
  Its shipped build is not measured but reported: nothing here queries Google
  Play, so `shipped:27` rests on cpmdroid's own record that Play refuses an
  upload at a versionCode it has seen and 27 is spent.
- **`romwbw_emu` `a95db9f`** - a row-by-row re-read of the whole column, which
  it had not had since 2026-08-24, fifteen commits earlier. That sweep never
  wrote down what it read, so this line said `unknown` until now. Twelve of the
  thirteen cells still stood as written; row 5 did not, and was rewritten from
  the page, the makefile and the release workflow rather than adjusted.

| Feature | iOS/macOS `ioscpm` | Android `cpmdroid` | Linux/Web `romwbw_emu` |
| --- | :---: | :---: | :---: |
| 1. Configurable keymap (termcap) | ◐ (26 keys: F1–F12 since build 51, the four Ctrl+arrows since `0165dac`, and all 26 reachable without a hardware keyboard from a three-page on-screen key row since `8e7587f` — shipped in build 61, suppressible in Settings, and on Catalyst the only way to send Ctrl+arrow at all; no other modifier bindings, lower-camel names, WordStar default) | ⬜ (no map at all: no key-map source file, no bindings, no Settings → Keyboard Map — `handleKeyDown` is a fixed `when` over keycodes with the bytes compiled in. That table is the VT220 set: F1–F12 out of `sendFunctionKey` and the Ctrl `'@'`–`'_'` window out of `controlByteFor` since `a523d40`; the nav cluster and Ctrl+arrow out of `sendArrow` since `c0b3bf7` — **not** all three at `c0b3bf7`, which is what this cell said. Hardware keyboard only: the on-screen strip offers Ctrl, Esc and Tab as keys (its other two buttons are Copy and Paste), so with no keyboard attached no F-key, arrow or nav key is reachable at all. Four of the nav presses are the app's rather than the guest's, also since `c0b3bf7` — Ctrl+Home/End and Shift+PageUp/PageDown scroll history) | ➖ CLI (host terminal) · ◐ web (xterm.js fixed map, not configurable) |
| 2. Scrollback | ✅ since ioscpm build 57 (2026-09-02), cleared at both fresh-session paths since build 58 (⬜ before 57: the LF path called `scrollRegion`, which does not capture, so no line ever entered the buffer from build 42 on) | ✅ since `e9436a5` (2026-09-02), and right in the case it advertises only since `35873d0` — the shipped 1.25 (capacity choices incl. Off, capture at `scrollUp`, both chord pairs, the view anchors, the cursor hides, and history draws with the soft keyboard up; keeping history across a cold boot is deliberate there, not a defect). `e9436a5` added `maxScrollLines()` and then used it in only some of the paths that move the view: `scrollUp`'s follow-along anchor still clamped to `historyChars`, so above the history — the only range a fresh boot with the keyboard up has — the first captured line pulled the view *forward*; and dismissing the keyboard *grew* the viewport, which shrank `maxScrollLines()` past an offset nothing re-clamped, so the next captured line pinned the view to the oldest history line with the cursor suppressed, the terminal looking frozen while CP/M kept printing — with Ctrl+Home/End still measured the same wrong way, a no-op on a fresh boot and, from that state, landing on the clamped maximum rather than back at live. `35873d0` bounds all of those on `maxScrollLines()`, adds `clampScrollToViewport()` at the three places the geometry changes, and lifts `sendChar`'s snap-back out as a public `returnToLive()` that the Esc and Tab buttons call — the control strip being the nearest keyboard in exactly the case the feature exists for. Never watched on a screen either way: the windowing arithmetic was swept in a scratch model, not driven on a device | ➖ CLI (host terminal) · ◐ web (xterm.js default buffer, no option set) |
| 3. Mouse/native Copy-Paste | ✅ iOS + Mac Catalyst in the shipped build 61 (`af0b9b2`, 1.5.1) — Mac pointer drag since build 57, iOS press-and-hold-then-drag since 61 (before it `handleSelectPan`'s sole call site sat inside `#if targetEnvironment(macCatalyst)`, so no iOS gesture could set an anchor and the menu's Copy silently took the whole screen). Linear span incl. scrollback, ⌘C and the menu's Copy take the selection, Copy All is the no-selection fallback; 61 also made the span inclusive (57–60 copied one character short of the drag) and put Paste in the long-press menu, gated on `hasStrings`. Two gaps on both platforms: paste is not gated on the emulator running, and a pasted CRLF never becomes Enter — `pasteText` maps a bare LF to CR, but Swift iterates CRLF as ONE `Character` matching neither branch, so `sendKey`'s `asciiValue` folds it to 10 (LF), not the CR CP/M needs. The iOS gesture has never met a real finger: synthetic simulator events only, `MANUAL_CHECKS.md` §17 unticked, no grab handles and no autoscroll past an edge, so one gesture never selects more than a screen | ◐ (control strip; `copyScreenToClipboard` takes history and screen, no selection) | ➖ CLI (host terminal) · ✅ web (xterm.js selection) |
| 4. R8/W8 arbitrary host paths | ◐ (R8 via Import File…; W8 fixed to `Exports`, and reports it since build 52; build 61 made `emu_host_file_open_read()` synchronous and moved the case-insensitive scan into it, so R8's `Reading:` line can at last carry the absolute path in the file's own spelling — but only for a disk carrying the `r8.com` that asks `0xEA`, which is the `v1.4.12` combo build 59 repinned to and **not** the `v1.4.5` one every earlier build shipped; an installed v1.4.5 image with no ledger entry is offered a lossy Update rather than refreshed, so an upgrading user still sees the shouted name. Same edit: a missing name or a directory now fails the open instead of leaving a zero-byte CP/M file, and a new 8 MiB cap refuses an import build 58 took in full. Unrun either way — `MANUAL_CHECKS.md` §14/§15 unticked, and the core checks use their own fake backends) | ✅ (File transfer screen, save-as, share sheet, import picker and an inbound share target since `71465cb`; folders still fixed, import capped at 16 MiB) | ✅ CLI (R8 any path; W8 `<cpmname> [hostpath]` since `98eb6a1`) · ✅ web (picker/download) |
| 5. Disk catalog + **pinned** tag | ✅ / ✅ pinned (`v1.4.12` since build 59 — **not** `v1.4.5`, which is what this cell said when it was read at build 58; one `releaseTag` at `EmulatorViewModel.swift:162` still builds both `catalogURL` and `releaseBaseURL`). Shipped build 61 = 1.5.1 carries the repin, so an installed client fetches the fixed-R8 catalog, and the builds 55/56 work is live rather than queued: every download hashed against the catalog inside `downloadDiskFromSettings` before it replaces anything and refused outright when an entry carries no `<sha256>` (all 20 do), the cache stamped with `catalogCacheTagKey` and salvaged down to already-installed entries on a pin mismatch, and `checkCatalogVersionAndInvalidate` → `deleteCatalogDisks` clearing only what the new catalog can give back. Build 60's `DiskLedger` adds per-file provenance and acts on it two ways: an Update control, allowed on any network but refused while the emulator holds the disk, and an automatic refresh of superseded images that additionally defers off Wi-Fi, on a constrained link, or while that disk is mounted. Help still floats on `releases/latest`, behind the bundled fallback of build 51 | ◐ / ✅ pinned (`v1.4.5` — still, in the shipped `35873d0` = versionCode 27 = 1.25: one `RELEASE_TAG` in `DiskCatalogRepository.kt` builds both `CATALOG_URL` and `DOWNLOAD_BASE_URL`, with the RomWBW v3.5.1 reason in the comment above it. `642b3b0` repins to `v1.4.12` for the R8 `F_DELETE` wildcard fix and is **not** an ancestor of the shipped commit, so an installed client still fetches the v1.4.5 images that ioscpm repinned away from at build 59; the interface-v0 migration `41829cb` and the ROM-from-catalog work `bb0ac74` are later still and have shipped nowhere). ◐ rather than ✅ because two of this row's spec items are missing from the app, and were missing at 25 too: **delete** — `deleteDisk` and `deletePersistedDisk` have no caller anywhere under `app/`, there are no menu resources, `DiskCatalogAdapter` binds no long-press, and the Settings slot ✕ is `clearDiskSlot`, which unassigns the slot and leaves the image on the device with nothing in the app able to remove it — and **cancel**, which is only the side effect of the Activity dying (`ensureActive()` inside `downloadDisk`'s read loop); the Settings progress dialog is `setCancelable(false)` and the first-launch overlay has no button. The downloader itself is stronger than this cell ever admitted, and all of it is live: `downloadDisk` hashes the stream as it writes and refuses on a SHA-256 mismatch, refuses a short transfer against the catalog `size` and `contentLength()` separately, publishes by renaming a nonce-named scratch file and checks `renameTo`'s answer, sweeps abandoned scratch by age, and `claimDownload` is static so Settings and `MainActivity` cannot pull the same disk at once. Two gaps beside ioscpm: the hash gate is `diskInfo.sha256.isNotEmpty()`, so an entry carrying no hash installs unverified where ioscpm refuses outright, and there is no cached catalog to stamp with the pin — `cachedCatalog` is one in-memory field in `SettingsActivity` that dies with it, so no cache-tag mismatch and no version-bump invalidation exist to get wrong. No provenance ledger, no Update control. Help still floats on `releases/latest`, behind the bundled fallback of `1f70c6b` | ➖ CLI (local paths only) · ⬜ web (no catalog and no tag: a hardcoded five-name `<select>` fetched beside the page, and nothing ships a single `.img` — neither deploy target nor the release workflow — so all five 404, both defaults included) |
| 6. Help system + offline fallback | ✅ / ✅ bundled since build 51 (download, cache, then the shipped copy) | ✅ / ✅ bundled since `1f70c6b` (download, cache, then the shipped copy; all eight files in `assets/help/`) | ◐ both (usage text / static panel, no topics — so no `releases/latest` trap either) |
| 7. NVRAM autoboot / bootString | ✅ NVRAM / ⬜ bootString (`setBootString` is in the vendored core and on the bridge; no Swift caller ever passes it a value, and there is no setting for one) | ✅ NVRAM / ⬜ bootString | ✅ CLI (`--boot`, NVRAM persisted) · ◐ web (set/clear, never read back) |
| 8. Window state / DPI | ◐ Mac Catalyst (position and size remembered across quits since build 61, landed in `8e7587f`: `WindowFrame`/`CatalystWindow` keep four numbers under `catalystWindowFrame`, clamp a restored frame to the display, drag an off-screen one back on and hold a 640×480 minimum through `sizeRestrictions`; saved on `scenePhase` deactivate, restored a turn after `onAppear`. Placement needs iOS 16's `requestGeometryUpdate` and `IPHONEOS_DEPLOYMENT_TARGET` is 15.0, below which neither position nor size comes back and only the minimum applies. Only the testable half is tested: `WindowFrame` has 34 checks, `CatalystWindow` none, and MANUAL_CHECKS §10's move/quit/relaunch and off-screen-restore boxes are both unticked — compiled for Catalyst, never driven on a Mac. Still no auto-size to the 80×25 grid on a font change, font size being a fixed 14–28 pt menu, and no per-monitor DPI scaling) · ➖ iPhone/iPad | ➖ | ➖ |
| 9. Font size setting | ✅ (menu, 14–28pt) | ✅ (Settings slider, 8–24pt; the range is enforced on the slider only, and API 24–25 ignore `android:min`) | ➖ CLI (host terminal; no font flag and no font config key) · ◐ web (fixed `fontSize: 16` in the `new Terminal` options, no control on the page and no font key among the six it persists, so only browser zoom moves it) |
| 10. Dazzler | ⬜ | ⬜ (explicit no-op stubs) | ⬜ (no Dazzler code; the core only offers the hooks this repo uses) |
| 11. Config profiles | ✅ since build 61 (`ProfileSection` in Settings, ungated so iOS and Catalyst alike: named `EmulatorProfile`s carrying ROM, four disk slots, boot string, key profile plus custom bindings, scrollback capacity, bell, manifest warning, key row and new-disk size; save, tap-to-apply, swipe-to-delete and update-in-place, the whole `ProfileStore` as one JSON value under the `emulatorProfiles` default, 66 checks in `EmulatorProfileTests`. ⬜ before 61 — the code landed in `8e7587f`, still stamped build 58, the same number as the `e33beea` reading that missed it, and the two builds that then carried it, 59 and 60, never became a binary anybody could install, so the changelog renumbers the entry to 61. Font size is the one setting a profile does not carry, living alone in `@AppStorage("terminalFontSize")`; file-backed local disks are recorded empty on purpose, a bookmark being a token and not a name, at the cost that an empty slot clears only the catalog selection so applying a profile can never detach a local disk; and `renameProfile` exists with no UI to reach it) | ⬜ (flat SharedPreferences, no named profiles) | ◐ CLI (one JSON settings file, v1.34; no named profiles) · ◐ web (one UI selection set) |
| 12. Manifest write warning | ✅ (suppressible, once per session) | ✅ (suppressible, once per session) | ➖ CLI · ✅ web (*Don't warn* kept across a reload since `108856c`) |
| 13. Terminal emulation (VT100 + VT52) | ✅ the origin of the parser (full VT52, DECSTBM, DECSC/DECRC, `@ P X L M S T`, the answerbacks, deferred autowrap, charset consumption, per-cell bold/underline/blink and the bright SGR halves since build 55) — and since build 61 the best-evidenced *shipped* parser of the four: at `af0b9b2`, the commit the App Store's 1.5.1 was built from, the whole parser has moved out of `EmulatorViewModel.swift` into a Foundation-only `TerminalScreen.swift` (`8e7587f`) with no final byte, mode or dispatch arm changed, and `TerminalScreenTests.swift` drives it headlessly in 262 checks, almost all through `receive(_:)`, the one door a guest has. CP/M 2.2 booted through the view on the simulator; nothing on hardware, and `MANUAL_CHECKS.md` §4's per-cell-face boxes are still unticked | ✅ **and shipped** — 1.25 = versionCode 27 (`35873d0`) carries the whole parser, so this is the one cell in this column where a Play user has what the tick describes. VT52 with auto-detection, DECSTBM, DECSC/DECRC (rendition and reverse travel with the position), `@ P X L M S T`, `G`/`` ` ``/`d`, `s`/`u`, the query replies with the private forms silent, DECANM/DECAWM/DECTCEM, deferred autowrap, `MAX_CSI_PARAMS` 16 / `MAX_CSI_PARAM_DIGITS` 6, and per-cell bold/underline/blink — bold and underline pick one of four `Paint`s through `CELL_FACE_MASK`, which is `CELL_BOLD or CELL_UNDERLINE` and deliberately does *not* include blink; a blinking cell keeps its face and collapses the glyph into its own background on the off phase. Reverse is resolved into the two colours at the write rather than kept as a cell bit, as in this repo. Written 2026-08-29: `167acbe` for the parser and the faces, `c0b3bf7` for the bounds, both ancestors of the uploaded bundle. **Built and served, still unwatched:** `MANUAL_CHECKS.md` §5 "first sighting" survives intact at all nine items, `todo.txt` still heads a list "NOBODY HAS SEEN THE NEW TERMINAL RUN", and there is no test source set in the tree, so nothing headless covers it either. Prints every byte from 0x20 up where both siblings stop at 0x7E | ➖ CLI (host terminal; output drops CR, masks to 0x7F) · ◐ web (page filter fixed in `2dbf6f2`; the wasm backend drops CR and masks to 0x7F the same way) |

**z80cpmw's own row 13 became ✅ on 2026-08-28**, which makes every row in this
document ✅ for z80cpmw — the other twelve by construction, this one on the
evidence in item 13. It was ◐ for one stated reason, no per-cell attribute
beyond the packed CGA byte, and that closed in `480edcb` and `29d3438`. Read the
✅ as "in the tree": the shipped **1.0.22** and **1.0.22-beta** packages are the
1.0.20 parser, without the attribute work and without the bright half of the
palette.

**The caveat that used to span the whole web column is closed, and was already
closed when this column was last read.** `xterm.js`, its CSS and the fit addon
were three jsdelivr `<script>`/`<link>` tags with no vendored copy and no SRI, so
offline - or from an installed deb - `new Terminal(...)` threw at top level and
there was no terminal at all. `5920681` (2026-08-26, one day before the reading
this block replaces) vendored all three under `web/vendor/`, and
`web/romwbw.html-template` and `web/romwbw-debug.html` load them from there;
`.github/workflows/release.yml` stages that directory - the three files and both
MIT licences - beside `romwbw.html`/`.js`/`.wasm`, `roms/*.rom` and `emu_avw.rom`,
so an installed deb or rpm has a terminal with no network at all. The only
`cdn.jsdelivr.net` left in that repo is `archive/cpm22/index.html`, which nothing
builds and nothing packages. The tags carry no `integrity=` and deliberately not:
SRI checks a file fetched from a host you do not control, and these are
same-origin files inside the package - `web/vendor/README.md` records the npm
tarballs the bytes came from and the three sha384 digests the old tags carried, so
an update can still be checked against what shipped. What is *not* closed is that
nobody has looked at the result: that repo's `todo.txt` no longer tracks vendoring
but carries a `[BROWSER]` item saying no browser has drawn the page since, and
`MANUAL_CHECKS.md` says the staged paths were checked by fetching every `href` and
`src` and no further. Nor can that be done from the source tree any more - `web/`
holds no `.wasm` at all since `2096ea2` and `95d422a` deleted the stale ones, so
`make serve` and both deploy targets fail on absent emcc rather than serving a
five-month-old build, and the checklist now says to serve the unpacked deb
instead. Rows 2, 3 and 13 are read from source, not watched.

## Suggested priority order for each GUI port

1. **Terminal emulation (#13)** — decides which software runs at all, so it
   outranks everything else. **All three ports now implement it**: `cpmdroid`
   caught up last, on 2026-08-29, and the three parsers agree on VT52, the
   scrolling region, deferred autowrap, the editing finals and the answerbacks.
   What differs is detail, listed per port above. This line was written before
   the Android half was true — it is one of the places the "a sweep of a column
   is not a sweep of the document" warning caught out — and it is now correct
   for the first time, with the one qualifier row 13 carries: the Android
   parser is compiled and unrun.
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
   left is the export half — a document exporter, which makes it the weakest
   cell in that row now. **Android closed both halves on 2026-08-29** (`71465cb`)
   and went past the target: a File transfer screen, save-as through
   `ACTION_CREATE_DOCUMENT`, a share sheet, an import picker, and an inbound
   share target so another app can push a file in. That last one is target (c)
   and Android is the only port with it — it is the worked example iOS should
   copy, in the way this list has been short of one since the `c26aeb7`
   reading was withdrawn.
5. **Pin the disk catalog to an explicit release tag (#5)** to stop HBIOS/CBIOS
   version drift. **Done on both** — Android and iOS/macOS are each pinned to
   `v1.4.5`. What is left is the other half of the same trap: help still
   resolves through `releases/latest`, which is only safe with a bundled
   fallback. iOS/macOS gained one in build 51 and **Android in `1f70c6b`**, so
   **this repo is the only port still exposed** — it has only its own two
   topics, and bundling the seven published ones is blocked on a wording
   decision rather than on work (#6).
6. Align **help topics / offline fallback (#6)** and **NVRAM/autoboot (#7)**.
   The fallback half of #6 is done everywhere but here. For Android the
   remaining piece of #7 is the `bootString` auto-type, which is the only open
   half of *this item* there — #1 above and the optional #10/#11 below are still
   open on that port.
7. Desktop-only: **window state + DPI (#8)** for the Mac build.
8. Optional: **profiles (#11)**, **Dazzler (#10)**.

For the Linux CLI, items 1/2/3/8/13 are mostly the host terminal's job; focus on the
catalog downloader (#5) and help (#6). The web/WASM frontend already covers
scrollback (#2), R8/W8 via browser picker/download (#4), same-origin disk
selection (#5, different model), UI-selection persistence (#11, lighter than
profiles), the manifest write warning (#12), terminal emulation (#13, xterm.js),
and adds a dirty-disk warning before tab close (v1.34).
