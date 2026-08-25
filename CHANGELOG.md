# Changelog

All notable changes to **z80cpmw** are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions use a simple `MAJOR.MINOR.PATCH` scheme: a `-beta` suffix names the
signed GitHub / sideload package, and the bare number names the Microsoft Store
release. The released Store version is **1.0.22**, published 2026-08-23; the
current sideload package is **1.0.22-beta**, which is the same binary signed
under our own publisher. **1.0.20** was built and tagged but never published on
any channel, and **1.0.21** shipped only as a signed sideload beta.

The two channels share a number only when they carry the same build, as 1.0.22
does. They remain separate package identities either way - the Store package is
`Publisher="CN=724C9014-..."` and the sideload package is
`Publisher="CN=Aaron Wohl, ..."` - so both can be installed at once and neither
replaces the other. Where the builds differ, the numbers must differ too.

Note that the Store channel is only partly visible from the repository: the
recent Store releases (1.0.19, 1.0.22) carry no git tag and no GitHub release,
while the older ones (1.0.10, 1.0.14) do, and a tag can exist for a version
published on neither channel (v1.0.20). `git tag` and `gh release list` are
therefore not evidence of what has shipped.

## [Unreleased]

The first pass on a Windows machine since the cross-port sweep that produced
`todo.txt`. That sweep was done on a Mac, so everything it found here was found
by reading; this is what compiling and running it turned up.

### Added
- **`emu_host_path_caps()`, and with it the sync to the `romwbw_emu` v1.36
  core.** The core declares this function and deliberately does not define it,
  so a port that syncs without supplying it fails to *link* - which is what
  happened here, and is the intended signal. It answers HBIOS `HBF_HOST_CAPS`
  (`0xE9`), the probe the new `W8.COM` makes before it will hand a host path to
  the emulator at all. This port returns `EMU_HOST_CAP_SAFE_PATHS`, honestly:
  the bit means a guest path is never used *destructively*, not that it is
  confined to one directory, and this backend's open-write is a plain
  `fopen(..., "wb")` that creates or replaces the one file the path names. It
  deletes nothing, substitutes nothing, and honouring an absolute path is the
  feature rather than the hazard. The design exists because the earlier shape -
  the core returning the bit as a constant - meant a port asserted the guarantee
  merely by compiling; `romwbw_emu` `docs/DOWNSTREAM_2026-08-25.md` §1b has the
  iOS data-loss bug that prompted the change.
- **`emu_rename()`.** `todo.txt` asked upstream for this shim and upstream added
  it, so `emu_io.h` now declares a function whose only definition lives in
  `emu_io_common.cc` - the one core file this port does not compile. Defined
  here over `MoveFileExA(MOVEFILE_REPLACE_EXISTING)`, which is what
  `emu_file_save()` already used inline, and `emu_file_save()` now goes through
  it. No behaviour change; it closes a declaration this tree had no definition
  for and makes the twelfth hand-synced function an obvious one.
- **Two headless suites for the host-file backend**, 102 checks, in
  `tests\run_tests.bat` beside the terminal one. `test_hostfile.cpp` compiles
  `emu_io_windows.cpp` on its own and tests the backend contract directly;
  `test_hbios_hostfile.cpp` drives `HBIOSDispatch::handleEXT()` with real guest
  registers and real guest memory, issuing the exact sequence `W8.COM` issues
  (`0xE9` probe, `0xE2` open, `0xE8` get-name, `0xE4`/`0xE5` write and close),
  so what is under test is what a CP/M program actually sees. The check that
  carries the most weight is not what the reported path *says* but that a file
  exists at it afterwards. Neither suite needs wxWidgets or vcpkg; the HBIOS one
  needs both sibling checkouts, since it links the Z80 core.
- **The terminal conformance suite, at last.** `[1.0.20]` announced a headless
  suite of 73 checks and said in the same breath that it was not committed; it
  never was, and the only trace it left was the reference to `test_vt52.cpp` in
  `TerminalView.h`. It is now in `tests/`, rebuilt from the `[1.0.20]` feature
  list and from the parser as it stands, at **252 checks**. It drives
  `TerminalView` the way a guest does - bytes in through `outputChar()` - and
  reads back only through the public interface: the screen through `cellAt()`,
  the cursor through `ESC [ 6 n`, which puts the answerback path under test
  rather than assuming it. It needs no window, and it needs neither wxWidgets
  nor vcpkg, so it runs on any machine with a compiler. `tests\run_tests.bat`.
- **Ctrl+arrow keys, and modifiers in the key map generally.** A binding is now
  identified by a virtual-key code *and* the modifiers held with it, so
  `Ctrl+Left` is a different binding from `Left`; the config accepts `Ctrl+`,
  `Shift+` and `Alt+` prefixes, stacked in any order. Defaults for the four
  Ctrl+arrows follow the same xterm convention as the rest of the table
  (`ESC[1;5A`..`D`). A modified press with no binding of its own falls back to
  the unmodified one, which is what every modified press used to get. `cpmemu`
  added its own Ctrl+arrows in `0b8dc2b`; this is the same gap closed in the
  shape this port's key map wanted.
  Alt needs care, because on Windows it is the menu key: an `Alt+` press is
  taken from the menu only when the user has bound that exact combination, which
  is the same rule F10 has always followed here. Nothing is bound to Alt by
  default, so the menu behaves exactly as before unless you ask otherwise.
- **The About box names the RomWBW release the core emulates.** A user who
  meets the "HBIOS/CBIOS Version Mismatch" banner is being told their disk
  images were built by a different release than this HBIOS emulates, and the
  app displayed nothing they could compare it against. Taken from
  `romwbw_pin.h`, which is the single source of that number.

### Fixed
- **`W8` was told the name it asked for, not where the file went.**
  `emu_host_file_get_write_name()` echoed the raw requested string, which on
  this port is two transformations away from the truth: a bare name is really a
  file in the data folder, and in an *installed* MSIX build that folder is
  really under `...\Packages\<family>\LocalCache\Local\`, because the OS
  redirects `%LOCALAPPDATA%` writes without telling the app. So the one place a
  CP/M user is told where their export landed named a path that does not exist -
  the "I can't find my exported file" question, asked and answered wrongly at
  the moment it comes up. It now reports the effective destination, resolved
  through `resolveHostPath()` and through the `GetFinalPathNameByHandle`
  redirection this port already followed for the About box and Settings, so all
  three agree. The core's tightened contract (`emu_io.h`) is honoured on both
  edges too: nothing is reported outside an open write, so the next `W8` cannot
  be shown the last one's destination.
  The redirection is followed through the *parent* directory rather than the
  file, which is not a detail: the existing resolver creates the path it is
  asked about, and pointing it at the file would have created a **directory**
  named after the export - after which the export's own `fopen(..., "wb")`
  fails. The test suite has that case, and it does fail without the split.
  Visible to users only once the disk images carry the `w8.com` that asks
  (`HBF_HOST_GETNAME`, `0xE8`); an older `W8` prints what it always did.
- **`ESC[2J` threw away the current colours.** Erase-in-display shares its
  implementation with the machine reset, so clearing the screen also reset the
  SGR attribute, the reverse-video flag and the escape parser's state - none of
  which erase-in-display says anything about. `clear()` is now the machine reset
  alone, and a new `eraseScreen()` is what `ESC[2J` and VT52 `ESC E` call.
  `ioscpm`'s `clearTerminal()` resets none of them either, which is what made
  this repository's `FEATURE_PARITY.md` row 13 the odd one out. The scrolling
  region is still deliberately preserved across an erase - that part of the old
  comment was right, and `ioscpm` is the one that gets it wrong.
- **A machine reset did not reset the terminal.** The flip side of the same
  sharing: because `clear()` was also the `ESC[2J` path, it deliberately left
  VT52 mode, DECAWM, DECTCEM and the scrolling region alone - so **Emulator >
  Reset** booted a fresh machine into whatever modes the last session had set.
  Now that the two paths are separate, a reset resets them.
- **Reverse video destroyed the colours it was supposed to restore.** `SGR 7`
  swapped the foreground and background nibbles of the attribute byte in place.
  The foreground is four bits and the background three, so the swap is lossy and
  `SGR 27` could not undo it: `ESC[1;31m ESC[7m ESC[27m` came back dim, and
  `ESC[7m ESC[1m ESC[27m` came back as a colour nobody asked for. Reverse is now
  a flag resolved at the cell write, and the stored rendition is never swapped.
- **`ESC[1;37m` was dim while `ESC[37;1m` was bright.** Setting a foreground
  colour masked out the intensity bit that `SGR 1` had just set, so bold
  survived only if it arrived last.
- **`ESC[m` was not `ESC[0m`.** It reset the attribute byte but left the
  reverse-video flag set, so a later `ESC[27m` un-swapped a byte that had never
  been swapped and put the whole terminal into reverse.
- **An erase painted the default colours instead of the current ones.** Every
  erase and every blank line scrolled in was filled with a hardcoded white-on-
  black, which was survivable only while `ESC[2J` also reset the rendition to
  exactly that. Once it stopped, a cleared region and the text written into it
  afterwards no longer agreed. `ED`, `EL`, `ECH`, `IL`, `DL`, `ICH`, `DCH` and
  the scroll paths all paint the current background now, which is what those
  sequences mean — a program can set a colour, clear, and get a screen of it.
- **`ESC[>m` reset the rendition.** The `m` case did not check for the private
  parameter marker, so xterm's `modifyOtherKeys` sequences were read as SGR;
  the bare `ESC[>m` was taken for `ESC[m`.
- **`ESC[38;5;44m` set a background.** Extended-colour subparameters were read
  as parameters in their own right, so the colour index landed as a CGA colour.
  There is nothing for this terminal to apply them to, but they have to be
  stepped over rather than misread.
- **`ESC 7` / `ESC 8` lost the colours.** DECSC/DECRC save the graphic rendition
  alongside the cursor position on a real VT100, which is what lets a program
  park the cursor, draw a status line in its own colours and put everything
  back. Only the position was saved, so the caller returned to the right cell
  wearing the status line's colours. `CSI s` / `CSI u` are the ANSI.SYS pair and
  correctly continue to save position alone.
- **Ctrl+@ / Ctrl+Space never reached CP/M.** `WM_CHAR` delivers 0 for them and
  the guard dropped it. NUL is a real byte a CP/M program can be waiting for.
- **Ctrl+J arrived as Enter.** `emu_console_read_char()` rewrote LF to CR on the
  way to the guest. Nothing needed it - `WM_CHAR` already delivers 0x0D for the
  Return key and the paste path maps `\n` to `\r` itself - and it destroyed the
  two cases that did: Ctrl+J, which WordStar-family editors bind, and any key
  binding written `"\n"` in `z80cpmw.json`. `ioscpm` dropped the same rewrite in
  its build 49.
- **A disk image larger than 128 MB was refused on Windows only.**
  `emu_file_load()` bounded whole-file loads with the size of the combo image
  this app *creates* rather than the largest one it can be handed; the shared
  `emu_io_common.cc` bounds them at 2 GiB. `EmulatorEngine::loadDisk` surfaced
  the refusal as "cannot read the file".
- **The NSIS installer built an app that could not start.** Two independent
  causes, both from the wxWidgets upgrade and both proved by building the
  installer and staging its output on a stripped path:
  - It packaged the wxWidgets DLLs *by name*, so it went on shipping the 3.3.1
    pair after the build moved to 3.3.3. Matched by wildcard now, so it follows
    whatever the build linked against.
  - It packaged `zlib1.dll`, which vcpkg's zlib port no longer produces - it
    installs `z.dll`, and `wxbase`, `tiff.dll` and `libpng16.dll` all carry a
    load-time import of it. The installer shipped a stale copy left in `bin`
    from the previous port and omitted the one actually imported, so the loader
    failed the whole chain with `ERROR_MOD_NOT_FOUND` before any of our code
    ran. Worse, once that stale file was cleaned up, `makensis` aborted outright
    rather than warning. The uninstaller still deletes `zlib1.dll` so an upgrade
    over an older install does not leave it behind.
- The rewind half of the shared `measure_stream()` was missing: `emu_file_load`
  did not check it and `makeDiskHandle` did not do it at all, leaving every
  freshly opened disk image positioned at EOF. `emu_file_load_to_mem` was on the
  same unchecked 32-bit `ftell`, and is now on the shared helper too.
- **A key that sent `0xFF` sent nothing at all.** `emu_console_queue_char()`
  took an `int` and every producer handed it a `char`, which is signed on MSVC,
  so a byte with the high bit set was queued sign-extended — and `0xFF` was
  queued as `-1`, which is exactly what `emu_console_read_char()` returns for
  "the queue is empty". Reachable from a key bound to `\377` or a boot string
  with a high byte in it. Queued bytes are masked now.
- **`ESC c` (RIS) did nothing.** The sequence a program sends to get a
  known-good terminal was swallowed by the escape parser's default case, while
  the function that implements exactly it — a full reset — already existed for
  the machine reset. The scrollback is deliberately kept: the history above the
  screen is the user's, not the guest's.
- **An override spelled in a different case lost to the default it replaced.**
  Bindings were merged by config name before being resolved, so `"CTRL+Left"`
  and `"Ctrl+Left"` were two entries for one key and ASCII ordering decided
  which was applied last. They are resolved to a key id before merging now.
- **`"Ctrl+Left": ""` sent `Left` instead of nothing.** An empty value unbinds,
  but an unbound *modified* key was merely absent from the table, so the
  fallback answered it with the plain key's sequence. An explicit unbind is now
  recorded as such and blocks the fallback.
- **`"F1x"` bound F1.** `atoi` stops at the first non-digit and reports what it
  read, so a name with a typo in it was silently bound to something near it
  rather than rejected.
- `ED 0` and `ED 1` left an armed autowrap armed, where `ED 2` cleared it.
- **A disk or ROM too large to allocate ended the process.** A disk image is
  read whole into RAM and then copied again into the HBIOS unit's buffer, so
  peak use is twice the file — and with the load bound now at 2 GiB that is a
  size a machine can legitimately fail twice over. Nothing above handled the
  exception. Both loads turn an allocation failure into the `false` their
  callers already model, and the ROM path says so in the error the UI shows.
- **A default key binding added after your config was written never appeared in
  it.** `z80cpmw.json`'s `keys` block is meant to be the visible, editable list,
  but it was only populated when it was entirely absent — so an existing user
  got no entry for the new Ctrl+arrows even though the app honoured them.
  Missing names are filled in individually now — matched by the key a name
  resolves to rather than by the name itself, so writing `Control+Left` does not
  earn you a second entry spelled `Ctrl+Left` for the same key. Every override
  survives, and a key deliberately unbound with `""` keeps its entry and stays
  unbound.

### Changed
- **`QKZ80_NO_TRACE` is defined for both configurations.** This port installs no
  tracer, so the decoder was paying a virtual call at every one of the core's
  202 trace points for a tracer nobody sets. `ioscpm` took the flag when
  `cpmemu` added it; this port did not.
- **The build is warning-free.** C4244 is silenced for the four imported
  `cpmemu` translation units and C4267 for `romwbw_emu`'s `hbios_dispatch.cc`,
  in both cases for the imported file alone. Each was audited first rather than
  assumed: the `cpmemu` ones are the Z80's intended mod-256 register wraps, and
  all six in `hbios_dispatch.cc` are a `size_t` loop index promoting a `uint16_t`
  guest address, where truncating to sixteen bits is the 64K address wrap HBIOS
  wants. Thirty-six permanent warnings are how a real one comes to be ignored.
- The repository's one test harness compiles again. `test_emu.cpp`,
  `compile_test.cmd` and `run_test.bat` all pointed at `z80cpmw/Core`, a
  directory deleted when the core moved out to the sibling checkouts, and
  `test_emu.cpp` was written against a `set_port_out_callback()` API the core no
  longer has. Repointed at `../cpmemu/src` and `../romwbw_emu/src`, and ported to
  the `port_in`/`port_out` overrides the core uses now.
- `emu_io_windows.cpp` says in its header that it is the Windows half of
  `emu_io_common.cc`, and lists the twelve functions that have to be kept in
  step by hand. Nothing reported that drift before.
- Removed `emu_disk_create()` and `emu_disk_create_memory()`, defined here,
  declared in no header and called from nowhere. Nothing in this port creates a
  disk image.

### Documentation
The cross-port sweep that produced `FEATURE_PARITY.md` was committed the same
afternoon as several of the upstream commits it describes, so parts of it were
stale within the hour. These are the corrections that have landed; the rows
still carrying the old text are listed in `todo.txt`.
- **Row 12 no longer says the web frontend forgets *Don't warn*.** `romwbw_emu`
  `108856c` moved the suppression call outside the `diskData` guard in
  `reloadDisks()`, so the checkbox survives a mid-session disk reload. This row
  had no stale paragraph of its own, so it is corrected outright.
- **Row 4 and row 13 are marked fixed upstream** in the header summary and in
  the per-port snapshot table: `98eb6a1` gave the `romwbw_emu` CLI
  `W8 <cpmname> [hostpath]`, and `2dbf6f2` opened `Module.onConsoleOutput` to
  every byte and implemented `Module.onError`. Each row's own paragraph still
  describes the old behaviour and is tracked in `todo.txt`.
- **The Android column is marked unverifiable.** It describes `cpmdroid` commit
  `c26aeb7`, which is not a valid object in that repository — `origin/master` is
  `7f46e98`, and none of the symbols the column cites (`DEFAULT_KEY_BINDINGS`,
  `decodeKeySequence`, `saveProfile`, any VT52 or DECSTBM code) exist there. A
  note at the head of the file now asks the reader to treat every Android cell
  as unverified until that branch surfaces.
- **`docs/FILE_TRANSFER.md`'s "I can't find my exported file" checklist no longer
  contradicts its own `W8` section.** Step 1 asked "Did you give a full path?
  (Windows only)" a hundred lines below the correction saying `W8` cannot take
  one; it now says which `w8.com` you have to have first. A step 0 was added for
  the thing that will make the rest of the checklist unnecessary: the refreshed
  `W8` prints the effective destination, redirection and all.
- **`FEATURE_PARITY.md` row 4's Windows half** records `emu_host_path_caps()`,
  the effective-destination reporting and the two suites that cover them. The
  row's own status is unchanged - none of it is user-visible until the images
  are refreshed. The terminal suite's check count is corrected there and in this
  file: it is 252, not the 205 both claimed.
- **`docs/FILE_TRANSFER.md` no longer promises a `W8` that takes a host path.**
  Its headline example, `W8 C:\Users\me\Desktop\out.com`, described the `w8.com`
  `98eb6a1` introduced, not the one inside the disk images this app ships —
  which reads only the default FCB and writes into the data folder whatever you
  type. The quick-reference table and the Windows section now say so, and say
  what to do until the images are refreshed. The same claim survives elsewhere
  in `README.md` and the in-app help; `todo.txt` lists where.

### Verified
- **The tree builds against the v1.36 core.** Both configurations, against
  `romwbw_emu` `17cd380` (`v1.36-1`) and `cpmemu` `9fee3c2`. The link error the
  sync was supposed to produce did produce - `unresolved external symbol
  emu_host_path_caps`, from `hbios_dispatch.obj` - and defining the function is
  what cleared it.
- **All three headless suites pass**, 354 checks: 252 terminal, 66 host-file
  backend, 36 HBIOS. Both new suites were checked against a deliberately broken
  build before being believed - reverting `emu_host_file_get_write_name()` to
  the old echo fails 9 of them, and resolving the redirection through the file
  instead of its parent fails 9 more, including the exports that then fail for
  real because a directory now sits where the file should go.
- **The tree builds.** Both configurations, against `romwbw_emu` `2dbf6f2` and
  `cpmemu` `9fee3c2`. This closes `todo.txt`'s "VERIFY FIRST - already pushed,
  never compiled" item: `0f3cc83`, which deleted the dead
  `emu_console_check_ctrl_c_exit` stub, had never been through a compiler. It
  compiles. The wxWidgets the project wants is `vcpkg install
  wxwidgets:x64-windows` (3.3.3), whose library names are exactly the four the
  project links.
- The two `hbios_dispatch.cc` fixes the sweep flagged - `bf03758`'s `HBF_VDAKST`
  and `HBF_VDAKRD` - arrive by reference and compile, as expected: this project
  builds that file straight out of `romwbw_emu`.

## [1.0.22] - 2026-08-23

The 1.0.21 code released to the Microsoft Store, and the first version published
on both channels from one build:

- `dist/z80cpmw.msix` - the Store package. Built unsigned with the Store identity
  (`CN=724C9014-...`); Microsoft re-signs it at distribution.
- `z80cpmw-1.0.22-beta.msix` - that same binary, byte for byte, repackaged under
  `CN=Aaron Wohl, ...` and signed with Azure Trusted Signing for sideloading.

Because the two are one build, they share the number; the `-beta` suffix and the
Publisher are what distinguish the packages. No functional change from 1.0.21.

## [1.0.21-beta] - 2026-08-23

The same code as 1.0.20, renumbered and published as the signed sideload beta
`z80cpmw-1.0.21-beta.msix`. 1.0.20 was built, tagged and packaged as an unsigned
Store MSIX but never reached a user through any channel, so its number was spent
rather than reused. No functional change from 1.0.20 - everything below is what
this build contains.

## [1.0.20] - 2026-08-22

### Added
- **VT52 terminal emulation.** The app is the terminal, so what it understands
  decides which CP/M software runs, and it understood no VT52 at all. The full
  set is now in: `ESC A B C D E F G H I J K Y Z <`, with `D`, `E` and `H`
  overloaded by mode, and direct cursor addressing (`ESC Y` with the row and
  column biased by 0x20). VT52 is auto-detected from any VT52-exclusive
  sequence, so ANSI output behaves exactly as before until one arrives, and it
  can be entered and left explicitly with `ESC [ ? 2 l` / `ESC [ ? 2 h` and
  `ESC <`. Ported from `cpmdroid`'s `TerminalView.kt`, itself a port of the
  iOS/macOS parser.
- **Answerback for terminal queries.** `ESC [ 6 n` (cursor position),
  `ESC [ 5 n` (status), `ESC [ c` and `ESC Z` (identify) now reply. A program
  that asked the terminal to identify itself previously waited for an answer
  that never came. Replies go straight to the guest rather than through the key
  path, which scrolls the view back to the live screen - the terminal answering
  a question is not the user typing.
- **Scrolling region (DECSTBM, `ESC [ t ; b r`)**, with LF, IND and RI
  honouring it. A partial region deliberately does not feed the scrollback:
  lines pushed out of a program's own status-line window were never history.
- **Deferred autowrap**, and DECAWM (`ESC [ ? 7 h` / `l`). A glyph written to
  the last column now leaves the cursor there and arms the wrap for the next
  character, as a real VT100 does. Writing the bottom-right cell used to scroll
  the screen immediately, which corrupts any full-screen layout.
- **The remaining CSI finals**: `G` and `` ` `` (column absolute), `d` (row
  absolute), `L`/`M` (insert/delete line), `@`/`P` (insert/delete character),
  `X` (erase character), `S`/`T` (scroll up/down).
- **Character-set and line-size designators** (`ESC ( ) * + #` and `ESC SP`) are
  consumed together with their parameter byte.
- SGR `22` (bold off).
- A headless conformance suite of **73 checks** covering all of the above. (The
  suite is not committed to this repository - the only test harness here is
  `test_emu.cpp` / `compile_test.cmd`.) `TerminalView` needs no window to parse bytes, so
  the test drives it through the public interface: it reads the cursor back with
  `ESC [ 6 n`, which puts the answerback path under test rather than assuming
  it, and screen content through the new `cellAt()`.
- `TerminalView::cellAt()` - a const, bounds-clamped read of a live screen cell,
  the counterpart to the existing `writeChar()`.

### Fixed
- **Ctrl+R rebooted the machine instead of reaching CP/M.** `^R` (0x12) is not a
  spare key: the CP/M command line retypes the current line with it, and
  WordStar-family editors bind it too. z80cpmw claimed it as the Reset
  accelerator, and `TranslateAccelerator` swallows a matched keystroke whole, so
  no `WM_CHAR` was ever generated and the guest never saw the character - it saw
  a cold restart from ROM, with no confirmation prompt and no way to switch it
  off. Reported by a user who typed it out of CP/M habit. Ctrl+R now goes to
  CP/M by default and Reset lives on the **Emulator** menu; `"ctrlRToCpm":
  false` in the `keyboard` block takes the shortcut back. F1 and F5 keep their
  old defaults, because CP/M has no function keys and reserving those costs the
  guest nothing.
- **A released shortcut needed a restart, and the menu kept advertising it.**
  The accelerator table was built once in `MainWindow::run`, so toggling
  `f1ToCpm` / `f5ToCpm` (or loading a profile that did) changed nothing until
  the next launch, and the Emulator and Help menus still showed the key. The
  table is rebuilt from `applyConfig` now, and the menu items relabel themselves
  to match what is actually registered.
- **Removed the dead `ACCELERATORS` resource** from `z80cpmw.rc`. Nothing loaded
  it - the real table has been built at runtime for some time - so editing it
  changed nothing, which is a trap worth not leaving in the file.
- **`ESC [ ? …` sequences printed their own tail as text.** `?` was treated as
  the final byte of a CSI sequence, so `ESC [ ? 2 5 l` ended at the `?` and
  `25l` appeared on screen. Nothing beginning `ESC [ ?` could work, DECANM
  included. `?`, `<`, `=` and `>` are now recognised as private-parameter
  markers, and intermediate bytes (0x20-0x2F) no longer end a sequence either.
- **`ESC [ ? 2 5 l` now actually hides the cursor** (DECTCEM), through a flag
  distinct from the blink phase the cursor timer owns.
- The secondary and tertiary device-attribute forms (`ESC [ > c`, `ESC [ = c`)
  stay silent rather than claiming to be a VT100.
- **`ESC [ 2 7 m` reset the whole attribute byte**, so turning reverse video off
  threw the colours away with it. It now undoes the swap, and a colour set while
  reversed is applied in the un-swapped domain so it lands in the nibble that is
  actually displayed. A second `ESC [ 7 m` no longer cancels the first.
- **LF now implies CR**, matching the iOS/macOS and Android ports. A
  Unix-format file `TYPE`d with bare line feeds used to stair-step down and off
  the screen.

## [1.0.19] - 2026-08-07

### Fixed
- **The `emu_romwbw.rom` in this repository was corrupt and could never boot.**
  Its HBIOS configuration block carried a wrong marker byte (`0xB8` where
  `~'W' = 0xA8` belongs), so a build that loaded it started a CPU that produced
  no output at all — no error, no boot menu, nothing. Replaced with the ROM
  rebuilt from source upstream; `emu_avw.rom` was intact but stale, and is
  refreshed from the same build.

  **No released build shipped that file, so no installed copy is affected.**
  Packaging reads `bin\Release\roms`, which held a separate hand-made snapshot:
  the ROM inside `z80cpmw-1.0.18-beta.msix` is a valid image (it is in fact a
  second copy of `emu_avw.rom`). The real defect this exposed is that the
  tracked ROMs and the packaged ROMs were two unrelated sets of bytes — see the
  staging fix below.
- **A ROM that fails to load now says why.** Three of the four ROM-loading
  paths ignored the failure: applying settings, loading the default ROM at
  startup, and applying a config profile all carried on silently, leaving the
  emulator with no usable ROM and no explanation. The reason now comes from the
  core — a corrupt HBIOS configuration block, or a ROM built for a different
  RomWBW release than this build emulates. Selecting a ROM from the menu and
  applying settings raise a dialog; the default ROM writes to the terminal; the
  config-profile path only writes to the log, because it runs at startup before
  the terminal is useful.
- **The build now stages `roms\` into the output directory.** Nothing ever
  copied them, so `bin\<Config>\roms` held whatever snapshot had been placed
  there by hand — in this tree, one from December 2025. That is how a release
  can fix a corrupt ROM and ship the corrupt one anyway: the app loads the ROMs
  from its own directory at run time, and `packaging/scripts/build-msix.ps1`
  packages from `bin\Release\roms`. A post-build step in both configurations
  now copies `roms\*.rom` to `$(OutDir)roms`.
- **Start no longer runs a machine with no ROM.** Every ROM-load failure left
  the emulator with empty ROM banks, and nothing recorded that: Start cleared
  the terminal — erasing the message that had just explained the failure — and
  ran a CPU over an erased bank, which is 0xFF everywhere, so `RST 38h`
  recursed on itself until the stack overwrote RAM. The status bar said
  "Running" throughout. `EmulatorEngine` now tracks whether a ROM is really
  loaded; Start refuses with the reason, and the menu item is greyed.
- **`SBC_simh_std.rom` is no longer offered, or shipped.** It is a stock ROM
  for real hardware with no port 0xEF HBIOS proxy, so it loaded "successfully"
  and then printed nothing forever — the same silent failure, on the one path
  the app advertised in its own menu. Upstream dropped the identical option
  from its web front-end this release. A config profile that still names it
  falls back to the default ROM and says so.
- **The Settings dialog no longer swaps the ROM behind your back.** It was
  never told which ROM was running, so its list always opened on the first
  entry and OK reloaded `emu_avw.rom` over whatever the user had selected,
  restarting the guest on different firmware. It is now seeded with the running
  ROM, only reloads on an actual change, and a successful change updates the
  ROM name, the menu check mark and the saved config — which previously kept
  the old value and reinstated it on the next launch.
- **Menu check marks are set after the menu exists.** `m_menu` was fetched
  after the two functions that set check marks had already run, so both wrote
  into a null handle and were then overwritten by an unconditional default.
- **The v1.35 core did not compile with MSVC.** Its hardened file I/O used the
  POSIX `fseeko`/`ftello`, which Visual C++ does not provide, so
  `romwbw_emu/src/emu_init.cc` failed with C3861 and this port could not be
  built at all. Fixed upstream in `romwbw_emu` by adding
  `emu_fseek`/`emu_ftell`/`emu_off_t` to `src/emu_io.h` — MSVC gets
  `_fseeki64`/`_ftelli64` and a 64-bit offset type, because its `off_t` is a
  32-bit `long` and would truncate offsets inside the range a combo disk image
  reaches. POSIX targets keep `fseeko`/`ftello` unchanged.

### Changed
- Synced to the **romwbw_emu v1.35** core, which pins the emulated RomWBW
  release (v3.5.1) in `src/romwbw_pin.h` and rejects a mismatched ROM at load
  time instead of running dead. `romwbw_pin.h` is added to the project file.
- The core's shared file I/O is hardened upstream this release; the fixes
  match the ones this port already made in its own `emu_io_windows.cpp`
  during the 1.0.18 migration audit, so nothing changes here.
- Version bumped to 1.0.19.

### Verified
- Built with MSVC (Visual Studio 18, x64): Debug and Release both compile and
  link with no errors.
- `romwbw_emu/roms/verify_romwbw_pin.sh ../z80cpmw` passes over the whole tree,
  including `bin\Debug` and `bin\Release`: every bundled emulator ROM is a
  RomWBW v3.5.1 emulator ROM, and the boot slices of the bundled disk images
  report CBIOS v3.5.1.
- The Release build starts, and the refreshed `emu_avw.rom` reaches the
  RomWBW boot loader (`RetroBrew SBC [SBC_simh_std] Boot Loader`).
- Selecting the old corrupt `emu_romwbw.rom` now raises an error dialog naming
  the reason ("no HBIOS configuration block at 0x103…") instead of starting a
  CPU that prints nothing.

- Start refuses, with the reason, when no ROM is loaded; the Start menu item is
  greyed until one is. Verified against a build whose ROMs are all corrupt.
- A saved ROM that cannot be used now reports itself in the terminal at
  startup, for both the default ROM and the one named by the config profile.

## [1.0.18-beta] - 2026-07-25

### Fixed
- **Settings window is now a singleton** — selecting Emulator ▸ Settings while
  it is already open now raises and focuses the existing window instead of
  opening a second copy that could drift out of sync.
- **Disk catalog loads again.** The catalog was pinned to ioscpm release
  `v1.4.5`, which had never actually been published, so every catalog fetch
  returned HTTP 404. Publishing that release restored the catalog with no app
  change. The bundled **Combo** disk now also carries the corrected `w8.com`
  (host file-transfer export names).

### Changed
- Synced the Z80 / HBIOS emulator core with **romwbw_emu v1.34** and fixed the
  bugs surfaced by the migration audit.
- Version bumped to 1.0.18, and the Store-identity package submitted to the
  Microsoft Store (unsigned; Microsoft re-signs at distribution).

## [1.0.17-beta] - 2026-07-14

### Added
- README **Download** section with the direct signed-beta MSIX link and a note
  that the Microsoft Store is still on 1.0.14.

### Changed
- Version bumped to 1.0.17.

## [1.0.16-beta] - 2026-07-14

First signed public beta. Cumulative over the 1.0.14 Store release.

### Fixed
- **Silent crash on F5 ([#1]).** Bundle the VC++ runtime and disable the
  constexpr `std::mutex` constructor so the emulator starts on machines without
  a matching system CRT.
- **Load / Save Profile crash ([#2]).** Also flush disk images to host storage
  on stop so changes are not lost.
- Additional latent silent-crash paths found while auditing #1.

### Added
- Crash reporting to capture faults that previously failed silently.
- Terminal scrollback (mouse wheel / Shift+PageUp).
- Signed beta MSIX build path (`-Beta`) via Azure Trusted Signing; installs
  side-by-side with the Store app and updates in place over earlier betas.
- Main-window position and size are remembered across runs; the window
  auto-sizes to the 80x25 grid when the font changes.
- Startup instructions moved into scrollable, offline **Help** topics (Getting
  Started / Configuration), with first-run auto-open.

### Changed
- Pin the ioscpm disk-catalog release tag for reproducible disk downloads.
- Documentation: boot / help text corrections and a feature-parity guide.

## [1.0.15] - 2026-06-23

Version bumped and documented; folded into the 1.0.16-beta package rather than
shipped on its own.

### Added
- Configurable keyboard map (termcap-style escape strings) for function and
  navigation keys, editable in `z80cpmw.json`.
- Mouse text selection with right-click Copy / Paste (Ctrl+C / Ctrl+V still
  reach CP/M unchanged).
- Configuration documentation in README, `docs/CONFIGURATION.md`, and online
  help.

### Fixed
- Correct the data-folder path shown in the About dialog.

## [1.0.14] - 2026-06-09

Microsoft Store release.

### Fixed
- Help-window font scaling on high-DPI displays.
- R8 / W8 host transfers now honor absolute paths; the resolved data folder is
  shown in About, Settings, and the boot banner.

## [1.0.13] - 2026-06-07

### Fixed
- Terminal font size on high-DPI displays.
- Installer build now picks the newest artifact when moving it into `dist/`.

## [1.0.12] - 2026-03-30

### Changed
- Adopt upstream romwbw_emu API changes.

## [1.0.10] - 2026-01-16

### Fixed
- Commit disk writes regularly for better write reliability.

### Added
- Setting to disable the manifest disk-write warning.

## [1.0.9] - 2026-01-15

### Added
- NVRAM persistence.
- Manifest disk-write warning.

### Changed
- Refactored the Cromemco Dazzler emulation (object-oriented rewrite).

## [1.0.8] - 2026-01-08

### Fixed
- Boot option using NVRAM switches.

## [1.0.7] - 2026-01-07

### Changed
- Use a single unified RAM-bank initialization bitmap so the port-I/O and
  SYSSETBNK paths no longer double-initialize (and the CBIOS page-zero stamp is
  always installed).

## [1.0.6] - 2026-01-07

### Changed
- Remove disk-slice limits and centralize the version number.

## [1.0.5] - 2026-01-02

### Added
- JSON-based configuration system with profiles.

## [1.0.4] - 2026-01-01

### Added
- Cromemco Dazzler graphics-card emulation.

## [1.0.3] - 2026-01-01

### Added
- Automatic disk slice-count calculation.

## [1.0.2] - 2025-12-27

### Added
- Remote help system.

## [1.0.1] - 2025-12-27

### Added
- Microsoft Store compatibility and an improved first-run experience.

[#1]: https://github.com/avwohl/z80cpmw/issues/1
[#2]: https://github.com/avwohl/z80cpmw/issues/2

[Unreleased]: https://github.com/avwohl/z80cpmw/compare/v1.0.22-beta...HEAD
[1.0.22]: https://github.com/avwohl/z80cpmw/compare/f91f0fd...v1.0.22-beta
[1.0.21-beta]: https://github.com/avwohl/z80cpmw/compare/v1.0.20...f91f0fd
[1.0.20]: https://github.com/avwohl/z80cpmw/compare/9a57708...v1.0.20
[1.0.19]: https://github.com/avwohl/z80cpmw/compare/0146d94...9a57708
[1.0.18-beta]: https://github.com/avwohl/z80cpmw/compare/v1.0.17-beta...0146d94
[1.0.17-beta]: https://github.com/avwohl/z80cpmw/compare/v1.0.16-beta...v1.0.17-beta
[1.0.16-beta]: https://github.com/avwohl/z80cpmw/compare/bbab303...v1.0.16-beta
[1.0.15]: https://github.com/avwohl/z80cpmw/compare/v1.0.14...bbab303
[1.0.14]: https://github.com/avwohl/z80cpmw/compare/7931317...v1.0.14
[1.0.13]: https://github.com/avwohl/z80cpmw/compare/6d81cc1...7931317
[1.0.12]: https://github.com/avwohl/z80cpmw/compare/v1.0.10...6d81cc1
[1.0.10]: https://github.com/avwohl/z80cpmw/compare/415adc7...v1.0.10
[1.0.9]: https://github.com/avwohl/z80cpmw/compare/eb97c64...415adc7
[1.0.8]: https://github.com/avwohl/z80cpmw/compare/ce25011...eb97c64
[1.0.7]: https://github.com/avwohl/z80cpmw/compare/6a0844c...ce25011
[1.0.6]: https://github.com/avwohl/z80cpmw/compare/711eccb...6a0844c
[1.0.5]: https://github.com/avwohl/z80cpmw/compare/7ff732f...711eccb
[1.0.4]: https://github.com/avwohl/z80cpmw/compare/a680b66...7ff732f
[1.0.3]: https://github.com/avwohl/z80cpmw/compare/e9cdb64...a680b66
[1.0.2]: https://github.com/avwohl/z80cpmw/compare/c3c7537...e9cdb64
[1.0.1]: https://github.com/avwohl/z80cpmw/commits/c3c7537
