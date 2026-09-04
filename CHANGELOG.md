# Changelog

All notable changes to **z80cpmw** are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions use a simple `MAJOR.MINOR.PATCH` scheme: a `-beta` suffix names the
signed GitHub / sideload package, and the bare number names the Microsoft Store
release. The released Store version is **1.0.23**; the newest sideload package
is **1.0.22-beta**, which is the same binary as Store 1.0.22 signed under our
own publisher. **1.0.20** was built and tagged but never published on any
channel, and **1.0.21** shipped only as a signed sideload beta.

**1.0.23 shipped to the Store; 1.0.24 is packaged and unsubmitted; 1.0.25 is a
tree.** 1.0.23 went out carrying the old catalog pin, so `1.0.24` exists to
deliver the repin that missed it, and `dist\z80cpmw.msix` is its unsigned Store
package, still awaiting a Partner Center submission. `1.0.25` is the version
number the work below it carries; it does not retire that package, because
1.0.24 is built and verified at the artifact and 1.0.25 is not yet built at all.
No `-beta` package has been cut for either. The Store channel leaves no git tag
and no GitHub release behind, so the repository is not evidence of what has
shipped - see the note below.

1.0.23's two packages carried **the same binary**, as 1.0.22's did:
`z80cpmw.exe` hashed `800715614bd5e20f…` inside both, because the beta was cut
with `-SkipBuild` off the build the Store package was made from rather than
being rebuilt. That is the rule for any version present on both channels - they
differ only where they must, `Publisher="CN=724C9014-…"` and unsigned for the
Store, `Publisher="CN=Aaron Wohl, …"` and Authenticode-signed for the sideload.
If a `-beta` is cut for 1.0.24 it must come off this same build the same way.

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

Nothing. `Version.h` says 1.0.25 and that release's entry is below; the detail
of what 1.0.23 carried is kept under this heading because 1.0.23's own entry
refers back to it.

### Released as 1.0.23

The first pass on a Windows machine since the cross-port sweep that produced
`todo.txt`. That sweep was done on a Mac, so everything it found here was found
by reading; this is what compiling and running it turned up.

A later pass on 2026-08-26 had no compiler at all — no MSVC, no wxWidgets, no
Windows, no PowerShell. Everything it added is marked **NOT COMPILED** or **NOT
EXECUTED** in its own entry and listed again at the top of `todo.txt`, because
an unmarked entry written blind is exactly the false `DONE` this project has
already had to correct once.

That pass also wrote three changes into the Windows sources - a `display.bell`
setting, a Reset confirmation, and keeping `applyConfig()`'s ROM notice across
`startEmulator()`'s terminal clear - and they have been **reverted**. Review
found the third one defective: `m_configNotice` is cleared only at the top of
`applyConfig()`, whose sole callers are `loadSettings()` and the profile load,
so a user who read "the saved ROM was not found", followed it, and picked a
working ROM through `onSelectROM()` would have had the stale error reprinted at
them. Writing blind into a tree with no compiler here produced a defect on the
first attempt, and patching it blind would only have stacked a second
unverifiable edit on the first. All three are open items in `todo.txt` again.

**Correction, 2026-08-28.** All three landed, on a machine with a compiler, and
the revert is the reason the third one is now right. `display.bell` is a real
setting with a checkbox on **Settings > Terminal**; Reset asks before it reboots
a running machine; and the ROM notices survive `startEmulator()`'s clear - but
not by being cleared at the top of `applyConfig()`, which is exactly the defect
the review caught. A notice lives as long as the claim it makes is true, so
`clearNotice()` is called from the four `loadROM()` success branches instead,
and a user who reads "the saved ROM was not found", follows it into
`onSelectROM()` and picks a working ROM retracts the notice by doing so. The
paragraph above stands as the record of why they were pulled; the entries below
are what replaced them.

A further pass on 2026-08-27, again with no MSVC and no Windows, wrote **two**
changes into the Windows sources. The first is
`emu_host_file_get_read_name()`, below. It is there because without it this
project does not link at all, so the choice was not "write blind or wait" but
"write blind or ship a tree that cannot be built". It is marked **NOT
COMPILED**, it is the smallest change that discharges the requirement, and it is
modelled line for line on the write-side function directly above it. The second
is the ANSI-to-CGA colour mapping, also below, and it is the one place on that
pass where being off a Windows machine cost nothing much: the parser needs no
window, so the real `TerminalView.cpp` was compiled here against a stubbed
`windows.h` and the conformance suite was actually run against it, in both the
fixed and the deliberately-unfixed direction. It is still not an MSVC build, and
it is marked accordingly. Everything else that pass produced is a shell script
or a document, and the scripts were run here.

The pass on 2026-08-28 is the first one here with MSVC, wxWidgets, PowerShell
and a screen all present at once, and its purpose was to settle what the three
blind passes could only mark. Twelve commits. Everything marked **NOT COMPILED**
above is compiled, and the one item that asked for a person to look at a real
screen and agree that `ESC[31m` is red is answered by a suite that reads the
pixels instead. The suites went from three to six and from 370 checks to
**1020**, all passing from `tests\run_tests.bat`: terminal conformance 516 (268
that morning), help renderer and assets 244 (new), rendering conformance 50
(new, and the only one that opens a window), host file transfer 66, HBIOS host
file extension 36, configuration diagnostics 108 (new).

What that total does not reach is worth naming before the entries rather than
after them. `MainWindow.cpp` and `SettingsDialogWx.cpp` are in no headless suite
and cannot be put in one - they need wxWidgets, a real window and an interactive
window station - so the Reset confirmation, the notice lifetime, the Settings
pages and the Keyboard page were verified by building the app into a private
directory and driving it with `WM_COMMAND` and `PrintWindow`, with the real
`z80cpmw.json` backed up and restored byte-identical afterwards. That is a
person watching a machine drive the app once; it is not a check that will fail
tomorrow if someone breaks it. The paint path is the exception: it lives in
`TerminalView.cpp`, and the rendering suite opens a window of its own, so bold,
underline and blink were read back as pixels rather than looked at. Where an
entry below names no suite, it was checked the first way.

`Version.h` still reads **1.0.22** and does not move here. 1.0.22 is published
on both channels, so a signed `-Beta` package run would overwrite a published
artifact under its own name; the number moves when this work is released. See
the `build-msix.ps1 -Beta` entry under **Changed** and the correction under it.

**Superseded 2026-09-03: `Version.h` reads 1.0.23** and everything above is
released under that number. See `[1.0.23]` below.

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
- **`tools/check-sibling-drift.sh`, which reports what `FEATURE_PARITY.md` is
  behind.** That document is a reading of four repositories taken on one
  afternoon, and three of them moved the same day; nothing said which parts had
  rotted, so the file was only ever as current as the last person to read all
  four trees. `FEATURE_PARITY.md` now carries a `sibling-readings` block naming
  the commit each port column was actually read at, and this script compares
  those with the checkouts beside this one, lists the commits that have landed
  since, and exits non-zero if any has. It also fails a recorded commit that is
  not an object in the tree it names — the `c26aeb7` failure caught
  mechanically instead of by argument. **This one was run**, against the real
  checkouts and against doctored input for each of its four outcomes; it needs
  only `sh` and `git`. Its verdict when it was written — `romwbw_emu`'s column
  with no recorded commit at all, both mobile columns one behind — is history:
  all three readings were brought forward in the same round and it exits 0
  today. See the origin-tip fix under **Fixed**, which is what made that
  reading trustworthy.
- **`emu_host_file_get_read_name()`, or this project stops linking.** **NOT
  COMPILED** — there is no MSVC, no mingw and no Windows on the machine this was
  written on. `romwbw_emu` `322ca8e` added the function to `emu_io.h` as a
  REQUIRED backend function and made `hbios_dispatch.cc` call it
  unconditionally for `HBF_HOST_GETRNAME` (`0xE9`'s read twin, `0xEA`); this
  project compiles `hbios_dispatch.cc` straight out of the sibling checkout
  (`z80cpmw.vcxproj`), and the symbol was defined nowhere in this repository —
  `grep -rn get_read_name .` returned only `CHANGELOG.md`. So the next build on
  a Windows machine would have failed at link, exactly as the
  `emu_host_path_caps()` sync did, and for the same reason: there is no version
  gate, the core simply arrives. `tests/run_tests.bat`'s HBIOS suite links the
  same pair and would have failed with it. `ioscpm` hit this first and its build
  was dead until `15f48e9`.
  It answers with **the path `R8` is really reading**, not `""`. That is the
  same treatment the write side already had: `emu_host_file_open_read()`
  resolves the guest's name through `resolveHostPath()` — a bare name is a file
  in the data folder, an absolute path is used verbatim — and the resolved path
  is now recorded in `g_hostReadDisplayPath` and reported. Where the write side
  must resolve the parent directory and re-join the leaf, because the file it
  names does not exist yet, the read side can resolve the *file*
  (`resolveRealPathExisting()`), which follows the same MSIX `LocalCache`
  redirection and additionally reports the leaf in the case the directory really
  holds rather than the case the CCP shouted. `ioscpm` answers `""` here and is
  right to: on that port the effective source is known only in Swift. On this
  one it is known, so `""` would have been a worse answer than the truth. The
  string is cleared at `emu_host_file_close_read()` and on every failure path,
  and `emu_host_file_provide_data()` clears it too — bytes arrive there with no
  path, and a stale one would name the previous transfer's file.
  What was actually done in place of a build: the added statements were lifted
  into a translation unit `clang++ -std=c++17 -Wall -Wextra` could parse, with
  the surrounding declarations given their real types, and they compile clean
  and behave as intended (the path while `READING`, `nullptr` after the close).
  That is a typo and type check, not a build of `emu_io_windows.cpp`, which
  needs `windows.h`. `todo.txt` keeps the item open until the tree compiles on
  Windows.
- **`packaging/scripts/verify-disk-assets.sh` — nothing checked what was inside
  the shipped disk images.** `build-msix.ps1` and `z80cpmw.nsi` copy
  `bin\Release\disks\*.img` into the package as they find them, no build target
  puts `r8.com` or `w8.com` on an image, and the images are not tracked here, so
  a stale utility ships silently and `git status` cannot notice. The script is
  pointed at a directory of release candidates and, per image, picks the diskdef
  from the image's own geometry (`wbw_hd1k_0` for a combo image, which must
  carry the `55 AA` MBR signature; `wbw_hd1k` for a plain 8 MB image, which must
  not), checks the directory it listed actually reads as a CP/M directory —
  cpmtools with the wrong diskdef prints an empty or blank one and exits 0,
  which reads as "the file is not there" — extracts `r8.com` and `w8.com` and
  compares them byte for byte against what `romwbw_emu/src/{r8,w8}.asm`
  assembles to, and asserts the `HBF_HOST_CAPS` probe bytes `06 e9 cf` are
  present in `w8.com`. That last check is the one a person cannot do by eye: a
  `W8` that takes a host path and never asks the emulator whether the path is
  safe prints the *same* usage line as one that asks, so the usage string does
  not discriminate armed from interlocked. It is the downstream half of
  `romwbw_emu/disks/verify_disk_utils.sh`, which checks that repository's own
  two images in place. **It was run**, both ways — see **Verified**.
- **The About box names the RomWBW release the core emulates.** A user who
  meets the "HBIOS/CBIOS Version Mismatch" banner is being told their disk
  images were built by a different release than this HBIOS emulates, and the
  app displayed nothing they could compare it against. Taken from
  `romwbw_pin.h`, which is the single source of that number.
- **Bold, underline and blink are per cell, and painted.** `TerminalCell` gains
  a flags byte carrying `TCELL_BOLD`, `TCELL_UNDERLINE` and `TCELL_BLINK`, set
  from SGR 1/4/5/6/22/24/25 and saved with the cursor, and `paint()` draws them
  from four GDI faces - `m_fonts[4]`, indexed by bold and underline - because
  GDI cannot turn weight or underline on for a single `TextOut`. The parser half
  and the paint half are deliberately separate commits, and the evidence that
  the first changed no pixels is that the rendering suite's checks did not move
  across it. Reverse video stays out of the flags byte: it is resolved into the
  colour nibbles at the write, which is what makes SGR 7 and 27 exact inverses.
  There is no italic face, because nothing can ask for one. `blankCell()` zeroes
  the flags, so `ESC[4m ESC[2J` does not underline all 2000 cells and
  `ESC[5m ESC[2J` does not strobe the whole screen - an erase paints the current
  colours, which is what lets a program set a colour and clear, but not the
  current face. SGR 21 is left in the default arm as a documented no-op, because
  ECMA-48 says double-underline, several terminals say bold-off, and nothing
  here can settle it.
  Blink shares the cursor's existing 500 ms timer rather than starting a second
  one that could drift out of phase with it, and the tick invalidates only the
  rows containing a blinking cell, so a screen with none repaints exactly as
  often as it did before. That guarantee needed a check of a kind the suite did
  not have - a window repainting twice a second to the same pixels looks like
  one not repainting, and counting `WM_PAINT` cannot see it either, because the
  cursor's own invalidation already produces one per tick - so the suite
  dispatches the tick itself and reads `GetUpdateRect`. The off phase is
  foreground = background, applied *after* the selection swap: collapsing first
  would leave a selected blinking cell blinking its highlight instead of its
  text. Tried in a scratch copy rather than reasoned about, and with the two
  lines exchanged the suite reports exactly one failure, in the section written
  for it.
- **The bell is a setting.** `TerminalView`'s `0x07` case called
  `MessageBeep(MB_OK)` with nothing to consult; `cpmdroid` made it a setting and
  this is the same thing here. `display.bell` round-trips in `AppConfig`,
  `setBellEnabled` / `isBellEnabled` / `setBellHook` are on the terminal, and
  there is a checkbox on **Settings > Terminal**. `applyConfig()` pushes the
  saved value at launch, which the item did not ask for and is the half that
  matters: `TerminalView` constructs with the bell on, so without that line a
  saved `"bell": false` would have been ignored on every start until the user
  next opened Settings and pressed OK - a setting that only works after you
  change it again. `clear()` deliberately does not touch it, because a guest's
  `ESC c` resets the machine and not the user's preferences. The hook is also
  what stopped the conformance suite beeping on every run since "BEL does not
  move the cursor" was written.
- **The Settings dialog is a notebook, and it has a Keyboard page.** Machine,
  Terminal, Disk Images and Keyboard, with every control reparented to its page
  panel. The restructure came first and added no settings and moved no handlers,
  because `wxCommandEvent` propagates up through the page panel and the event
  table is byte-identical; `m_statusText` stays on the dialog below the
  notebook, since six handlers write it and they do not share a page. The
  Keyboard page lists every bindable key with what it sends and a status, a box
  to edit the sequence, **Default** and **Unbind**, and the three app-shortcut
  switches. Rows are keyed by the id a name *resolves* to, so the
  `"Control+Left": "^A"` already in the config on this machine shows as one row
  rather than as a stranger beside `Ctrl+Left`; unrecognised entries are carried
  through untouched, which is what `KeyMap::build`'s refusal to filter was for;
  and the whole write-back is gated on a dirty flag, so an OK from someone who
  never opened the page cannot rewrite `keyboard.keys` at all. The four reserved
  combinations are greyed rows carrying `reservedPurpose()`'s words - absent,
  they are a mystery.
  `Keymap.h` gained `baseNameForVk()`, `nameForKeyId()` and `validateSequence()`
  so the page's questions could be asked somewhere a test can reach. The names
  written back are `defaultBindings()`'s spellings, because that is what a fresh
  `z80cpmw.json` contains and therefore what a user reading their own file sees,
  and the modifier prefix order is fixed at Ctrl, Shift, Alt - which is not
  formatting. `keyIdForName` accepts the prefixes in any order, so a writer that
  did not choose one order and then always choose it would add `Ctrl+Shift+F3`
  beside an existing `Shift+Ctrl+F3` and leave two entries for one key, with the
  winner decided by `KeyMap::build`'s merge order rather than by anything
  visible.
- **`keymap::reservedKeys()`, one list instead of four hand-written `if`s.**
  Shift+PageUp, Shift+PageDown, Ctrl+Home and Ctrl+End drive the scrollback
  viewer, and `handleKeyDown` answered them above its `m_keymap.find()`, so a
  config that bound one of them was ignored in silence rather than refused, and
  nothing outside that function knew the list - `docs/CONFIGURATION.md` restated
  it in prose and a Settings dialog would have restated it a third time. The
  table carries one row per combination with the words a dialog puts beside a
  row it will not let you edit, and `reservedFor()`, `reservedPurpose()`,
  `isReservedForApp()` and `classifyName()` ask of it; `classifyName()` answers
  Ok, Unknown, or Reserved with the reason attached and the id still reported,
  so a line can be refused out loud and pointed at instead of dropped.
  `KeyMap::build` is untouched, deliberately, and now says why: being reserved
  is a fact about the user interface, not about the map - the app wins the press
  in `handleKeyDown` and the stored sequence simply never fires. The modifier
  test stays a mask rather than an exact match, which is what the replaced code
  did, so Ctrl+Shift+PageUp still scrolls back; narrowing it would have been a
  behaviour change no test pinned, made under cover of a refactor. The suite
  iterates the table itself rather than a fourth hand-written copy of the same
  four rows.
- **The configuration file reports what it holds that nothing reads.**
  `ConfigManager` subtracts the schema from the document and collects five kinds
  of problem - unrecognised setting, wrong kind of value, unknown key name,
  reserved key name, could not be read - and `renderBlock` states the fate of
  each kind separately, because they differ: an unrecognised member really is
  dropped at the next save, an unknown or reserved key name is *kept* (`to_json`
  writes `"keys"` back whole and nothing prunes it), and an unreadable file was
  not read at all. One sentence for all of them, which is what the first draft
  had, was false of a key binding and contradicted the note this repository had
  just added to `KeyMap::build`. nlohmann 3.11.3 keeps no record of which
  members a `from_json` consumed, so the schema has to be supplied rather than
  observed: `referenceDocument()` is built from `to_json`'s own output, which
  makes the writer the single source of the schema and turns any disagreement
  into a test failure, with two further canaries for what that cannot catch
  structurally - a member the writer emits that no reader reads, and a default
  literal that differs between the pair.
- **The startup notices have a lifetime, and the configuration report is
  shown.** `startEmulator()` clears the terminal three lines below its own
  no-ROM guard, and the guard cannot cover for it: the case that matters is a
  saved ROM that is unusable while `loadDefaultROM()` has already put a good one
  in the banks, where `hasROM()` is true, the guard does not fire, and the lines
  saying which ROM is actually about to boot are wiped. The five ROM messages
  and the configuration report go through `setNotice()` into an ordered map now,
  and `printNotices()` runs after both of this file's
  `clear(); resetScrollback();` pairs. `setNotice()` prints as well as
  remembers, because a notice raised where nothing is about to clear the screen
  would otherwise sit unread until the first Start.
  `ConfigManager::diagnostics()` had no reader at all;
  `reportConfigDiagnostics()` runs between `load()` and `applyConfig()`, and
  again on `loadProfile()`, and raises one notice per problem kind rather than
  one for the whole report, because the kinds stop being true at different
  moments.
  The could-not-be-read notice is the interesting one, and an adversarial pass
  caught it being retracted wrongly. That block is the only place in the UI that
  shows the backup file's name and the parser's line and column, and the
  retraction was not merely reachable but ordinary: a config that will not parse
  leaves `cfg.disks` empty, so the first Start routes through
  `downloadAndStartWithDefaults`, whose both-disks-present branch calls
  `saveSettings(); startEmulator();` back to back - retracting the notice one
  statement before the `printNotices()` that exists to survive that very clear.
  It is conditional on the quarantine having actually happened, because in the
  one case where the rename *failed* the original really is still there and the
  save really does replace it. Checked both ways on screen, including with all
  twenty backup names pre-taken.
- **Reset asks first.** `onEmulatorReset()` cleared the terminal and called
  `reset()` with no guard, and both mobile ports ask. It asks when the machine
  is running and not when it is stopped, because `onEmulatorStart()` already
  cold-boots with no confirmation and confirming Reset-while-stopped but not
  Start would be arbitrary. `MB_DEFBUTTON2`, so Enter cancels. Driven end to end
  to check it: the dialog appears, **No** leaves the typed character on screen,
  **Yes** really reboots, and a stopped machine gets no dialog at all. A profile
  that could not be read used to be quarantined in total silence, vanishing from
  the Load Profile list with nothing said but "Failed to load profile."; the
  failure branch reports now, and the message box says the file could not be
  read, points at the terminal for the reason, and says current settings are
  unchanged.
- **`z80cpmw/HelpAssets.{h,cpp}`, and help topics that survive going offline.**
  The state-free half of the help system - `HelpTopic`, `parseIndexJson`,
  `markdownToText`, plus a new `isSafeAssetName` and `toWide` - moved into
  `namespace help_assets` so it could be built against a test, and then gained a
  disk cache: `cacheDir`, `cachePath`, `cacheTempPath`, `readCached`,
  `writeCached`, `resolveTopic` and `sourceLabel`. `HelpWindow`'s in-memory
  cache held a topic for fifteen minutes and lost it at exit, so a reader who
  read the CP/M 2.2 guide yesterday and opened it on a train today got "This
  topic could not be downloaded." `resolveTopic` is the one place the order
  lives - download, then cache, then the copy in the binary - and the status
  line names which copy is on screen ("(downloaded)", "(offline copy, saved
  ...)", "(bundled with the app)", "(this session's copy)"), because a reader
  who cannot tell a fresh topic from one saved before a release cannot judge
  what they are reading; `displayContent` stops writing a status of its own,
  since it is handed markdown and cannot tell where the markdown came from.
  Writes go to `<asset>.<pid>.tmp` and are renamed onto the real name, so the
  name a reader reads appears with all of its content or not at all: the file
  being overwritten is the only offline copy the user has, and a truncating open
  that then fails leaves them with less than they had. The pid is in the scratch
  name because two copies of z80cpmw can run, and an empty body is refused,
  because a blank pane is indistinguishable from a topic that loaded and said
  nothing. `isSafeAssetName` finally has callers, which is what it was written
  for: it is the first statement of both path functions, so there is no way to
  reach the file system without it having said yes, and `fetchTopic` refuses to
  build a download URL out of a name it rejects rather than pasting a string
  that arrived over the network into `WinHttpCrackUrl`.
  **The bundled copy did not land in this entry; it landed later on the same
  day, once the wording question below was answered upstream.** As written here,
  `resolveTopic`'s third step exists and `bundled` is empty for every topic that
  reaches it, so at this point the cache is the whole of the offline story. It
  was blocked on a decision rather than on work: three of the seven published topics
  are worded for iOS ("tap the gear icon", "Cmd+C copies the screen text", an
  "iOS / iPadOS" heading) and the published `help_index.json` calls Quick Start
  "Getting started with iOSCPM", so compiling them into a Windows binary would
  make the wrong wording durable and offline, which is the opposite of what the
  item wants. Somebody has to decide whether the text is forked here or fixed
  upstream in `avwohl/ioscpm`; `cpmdroid` forked in `78e6ec6`, which is the
  precedent, and `todo.txt` held it as a decision. Nothing called
  `help_assets::setCacheRoot()` at this point either - it is a seam, not a
  fourth copy of the `SHGetKnownFolderPath` snippet this repository already has
  three of - so `cacheDir` computed its path from the environment. Both of those
  were closed later the same day: the wording was fixed upstream rather than
  forked, and `MainWindow::onCreate` now makes the call. An MSIX install needs no change, because writes to `%LOCALAPPDATA%`
  are redirected by the OS exactly as the config file and the disk images
  already are, and both the scratch file and its target sit in one directory so
  the rename stays inside it - argued from how the existing writes work, not
  measured inside a package, and the comment says which.
- **Three new suites, and `tests\run_tests.bat` now runs six.**
  `tests/test_render.cpp` (rendering conformance, 50 checks) creates a real
  window, drives the parser with real bytes, asks the DWM to render the window
  with `PrintWindow(PW_RENDERFULLCONTENT)` and samples the bitmap;
  `tests/test_help.cpp` (help renderer and assets, 244) and
  `tests/test_config.cpp` (configuration diagnostics, 108) need neither a window
  nor the sibling checkouts, so they are wired in ahead of the two blocks that
  `exit /b 1` when a sibling checkout is missing - a suite appended at the end
  is unreachable on a machine that has only this repository. The rendering suite
  writes its palette out independently of `TerminalView::cgaToRGB()` on purpose,
  because a test that reads the same table it is checking asserts nothing, and
  it prints SKIP and exits 0 where there is no interactive window station, so it
  cannot turn CI red for want of a desktop. One detail in it is worth keeping:
  the font is created with `CLEARTYPE_QUALITY`, so a glyph's pixels are
  subpixel-blended and almost none is exactly the requested colour, which makes
  "which colour is this cell" a nearest-neighbour question asked of the pixel
  furthest from the background rather than an equality test.

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
- **A program asking for red got blue.** An `SGR` colour parameter carries an
  *ANSI* colour index - 1 is red, 4 is blue - and the parameter was stored
  straight into an attribute byte whose palette is *CGA*-ordered, where 1 is
  blue and 4 is red. The two orderings agree on black, green, magenta and white
  and disagree on the other four, because red and blue trade places: `ESC[31m`
  drew blue, `ESC[44m` filled red, `ESC[33m` drew cyan and `ESC[36m` drew
  brown. Any CP/M program that colours its screen - a menu, a status line, a
  Turbo Pascal `TextColor` - came out with half its palette wrong, and wrong
  consistently enough to look like a choice rather than a bug.
  The translation is now a named `ansiToCGAColor()` at the top of
  `TerminalView.cpp`, applied at the `SGR` parse site and *nowhere else*. The
  attribute byte stays CGA-ordered on purpose and that is not an accident of
  this code: a guest can hand the emulator a raw CGA attribute byte through the
  HBIOS VDA "set attribute" call (`HBF_VDASAT` -> `emu_video_set_attr()` ->
  `TerminalView::setAttr()`), and `cgaToRGB()` is a CGA palette. So no palette
  is reordered, the renderer is untouched, and the guest-attribute and
  blank-cell paths are untouched. The default `0x07` does not move either - 7
  maps to 7 and black maps to black, so a reset lands where it always did - and
  the intensity bit `0x08` is not a colour index and never goes through the
  mapping. This brings the port in line with `romwbw_emu`'s web frontend, which
  renders through xterm.js and has always read `SGR` colours as ANSI; `ioscpm`
  is being fixed the same way, and takes this port's `0xF8` mask with it.
  **NOT COMPILED with MSVC, and the app has not been built or run.** What *was*
  done, on a Mac with clang: (a) the mapping is a pure function, so it was
  copied verbatim into a standalone program and all eight indices proved against
  the table - `0->0 1->4 2->2 3->6 4->1 5->5 6->3 7->7`, its own inverse, never
  a result outside 0-7; (b) the real, unmodified `TerminalView.cpp` was compiled
  against a stub `windows.h` - every Win32 entry point a no-op, since the parser
  and the screen buffer touch none of them - and the whole conformance suite was
  *run*: **268 checks, 0 failed**; (c) the same suite was run against a copy of
  `TerminalView.cpp` with the mapping backed out, and **36 failed**, each one
  reporting exactly the expectation this change replaced. That last run is the
  part worth trusting: it is what says the new expectations are the *post-fix*
  ones and not a set of numbers written to agree with themselves. Nothing here
  has been seen on a screen.
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
- **`ESC[91m` drew in grey.** SGR 90-97 and 100-107 were not handled at all:
  they fell through `applySGR()`'s default and left the attribute byte alone, so
  a bright colour from a fresh reset drew in CGA 7. That was measured by the new
  rendering suite on its first run, not inferred, and it is the gap that suite
  earned its place by finding. The bright half is the same ANSI index with the
  intensity bit set, which is the bit SGR 1 already sets, so `22` dims a bright
  colour and a `3x` after a `9x` deliberately does not - that consequence of the
  earlier `0xF8` fix is pinned by a check now rather than left to be
  rediscovered. 100-107 fold onto the plain background, because the background
  nibble is three bits wide and the fourth is blink on real hardware: a wrong
  shade beats a cell that starts strobing. `cpmdroid`'s `TerminalView.kt` has
  had the branch since its own ANSI fix; `ioscpm`'s `applySGR` has the same hole
  - `EmulatorViewModel.swift` handles 30-37 and 40-47 only - and `todo.txt`
  notes it for whoever is next at a Mac.
- **The disk downloader was unreachable from Settings at 200% scaling.**
  Measured, not suspected: the single column's sizer wanted 850x1320 while the
  constructor pinned `SetSize(900, 750)`, `GetWindowRect` put the catalog list
  at 844x0, and a `PrintWindow` capture showed the whole download section -
  header, folder path, list, **Download** and **Delete**, progress - not
  rendering at all, with the Dazzler box clipped mid-group. A fixed pixel count
  falls further short the more the display scales. `Fit()` derives the height
  now: the tallest page is Machine at 762x559, the dialog settles at 900x819,
  and the list gets its 250px back. The notebook border and the page inset do
  cost the list 36px of content width, widening a column overflow that already
  existed. Two more of the same family were caught before they shipped - the
  constructor took `Fit()`'s answer as both the size *and* the minimum, so a
  dialog taller than the screen could not be made shorter, and the Keyboard page
  took the fitted height from 819 to 1105 against a 1366x768 laptop's 728px work
  area, while the key list's height was a raw pixel count that stayed 420
  however small the screen. The height is clamped to the monitor work area, the
  list height is `FromDIP`, and the notebook is what gives way rather than the
  status line and OK/Cancel sliding off the bottom. Checked by forcing the
  window to 1024x768 and to 300x200 and measuring where the buttons landed.
- **A disk catalog arriving from the worker thread put back everything you had
  just ticked.** `onCatalogLoaded` called `loadSettings()` to reapply the disk
  dropdowns after `populateDiskLists()` emptied them, and `loadSettings()`
  resets *every* control from `m_settings` - from a posted event seconds after
  the dialog opened, so anything changed in the meantime was silently undone. It
  repopulates the dropdowns alone now.
- **A mistyped key in `z80cpmw.json` was absorbed in silence and then deleted in
  silence.** `from_json` reads the fields it knows by name and `to_json` writes
  only those, so an unrecognised member was read by nobody and gone at the next
  save, and `KeyMap::build` discarded any name `vkForName` rejected without a
  word. Both are reported now - see the diagnostics entry under **Added** - and
  three cases in that family were worth their own measurement. `"keys"` written
  as an **array** loaded clean, left the keymap empty, and the very next save
  replaced every custom binding with the defaults, in the same launch:
  `collectMemberProblems` recursed only where both sides were objects or both
  arrays, which coincides exactly with the three places `from_json` guards on
  type instead of throwing (`disks`, `dazzler`, `keyboard.keys`), so a member of
  the right name and the wrong type was invisible. `"F13"`, `"PgeUp"` and
  `"Ctrl_Left"` resolve to nothing, so the binding never fires while the line
  looks exactly like a working one from every other angle. And a reserved name
  resolves but loses the press to the app.
  This is not the whole of that bug, and the entry says so rather than implying
  otherwise. The save suppression covers `load()`'s own save, which is enough
  for a file that could not be read, because the quarantine has renamed it away
  - but a wrongly-typed section is not quarantined, so a *later* explicit save
  (`saveWindowPlacement()` at `WM_CLOSE`, the welcome flag, an NVRAM change)
  still writes the defaults over it. Measured: a `keyboard.keys` written as an
  array lost the user's binding within twelve seconds of boot. Fixing it
  properly means carrying the unread text through to the next `to_json`, and
  `todo.txt` keeps it open.
- **A config file that could not be read was saved over.** `load()` ignored
  whether the read had succeeded: on a failure `m_config` was left default, the
  fill-in-missing-bindings loop found every binding absent and set `needSave`,
  and `save()` landed on the original bytes. The rule now is that we do not save
  over a file we failed to read, and it is a guard at the one place that can
  break it. That matters more than the quarantine it backs up, because the
  quarantine can fail: the file is renamed to `z80cpmw.json.bad` first, but a
  rename can be refused by an ACL, a sync agent or a handle held without
  `FILE_SHARE_DELETE`, and the previous draft then wrote defaults over a file it
  had neither read nor moved. A file that could not be *opened* took an early
  return ahead of all of this and produced no diagnostic at all - the one case
  where "could not be read" is literally true was the case with no report - and
  now carries its `errno` reason through the same path. The backup name no
  longer clobbers either: `.bad`, then `.bad2`, `.bad3`, to a cap of 20, after
  which the file is left alone, because the copy worth keeping is the first one,
  made when the settings were still the user's, not the near-defaults file a
  second failure would replace it with.
- **Five defects in the help renderer**, each measured over the eight assets
  published in `avwohl/ioscpm` rather than reasoned about, and fixed before
  bundling could make them durable and offline. `markdownToText` had no case for
  a fence at all, so both marker lines fell through to the ordinary-text branch
  and printed their own backticks - 170 such lines across the eight assets, 60
  of them in `help_cpm22.md` - and a fence is consumed now with what it encloses
  emitted verbatim, which is the point: a dash inside a fence is a diff line,
  not a bullet. The bullet branch emitted its marker and ran `continue` *before*
  the bold and backtick passes at the bottom of the loop, so a bullet carrying
  inline markup showed its markers, and the table and header branches had the
  same shape and the same fault - 63 bullet lines carry markup, and six table
  rows in `help_quick_start.md` carry backticks. The two passes are a lambda
  called from all four places that emit text, so no future branch can forget
  them. `displayContent` widened the rendered text with a char-by-char copy into
  a `std::wstring`, and `char` is signed on MSVC, so every byte over 0x7F became
  a `wchar_t` up near 0xFF80: exactly one published asset is not ASCII,
  `help_file_transfer.md`, 30 bytes making 10 characters, and each came out as
  three pieces of garbage. `toWide` calls `MultiByteToWideChar(CP_UTF8, ...)`
  and the three other narrowing sites go through it, deliberately without
  `MB_ERR_INVALID_CHARS`, so a stray byte becomes U+FFFD and the reader still
  gets the rest of the topic where a failed call would hand the pane an empty
  string. `fetchTopic`'s failure arm posted a status line and never a body, so
  the pane went on showing the previous topic and a failed click looked like a
  click that had not registered. Fixing that exposed the fifth: the local-topic
  branch sits *above* the `m_loading` guard, so selecting a bundled topic while
  a download is outstanding really does move `m_currentTopicId`, which means an
  unconditional repaint on failure would have replaced a topic the reader had
  just chosen with an error about one they had left. The content message carries
  the topic id now and is dropped unless it still matches, the success arm is
  tagged the same way, and the cache-hit branch that never assigned
  `m_currentTopicId` at all - wrong title in the status bar on every cache hit -
  assigns it.
- **A truncated download was reported as a complete topic.**
  `downloadToString` ended its read loop with an unconditional success: a
  `WinHttpQueryDataAvailable` that failed broke out as though the body had
  ended, a `WinHttpReadData` that failed skipped its chunk and the loop carried
  on, and nothing compared the assembled length to anything. Before the cache
  that meant one pane showing half a topic for one session; with a cache behind
  it, `resolveTopic` would report Downloaded, `writeCached` would replace a
  complete offline copy with the fragment, and the status line would call it
  "(downloaded)" - the truncation becomes the durable copy. Both halves of the
  new check are needed, and which half catches what was measured rather than
  assumed: a server that announces `Content-Length: 5468`, sends 2000 bytes and
  closes produces **no** WinHTTP error at all, because the next
  `WinHttpQueryDataAvailable` returns TRUE with 0 available, identical to a
  clean end of body - so treating a failed read as failure would not have caught
  the reported bug - while a truncated *chunked* response does raise
  `ERROR_WINHTTP_INVALID_SERVER_RESPONSE` and has no `Content-Length` to compare
  against, so the length check alone would not have caught that one. A missing
  `Content-Length` is therefore deliberately non-fatal: "no length, no document"
  would kill remote help the day the host switched to chunked. What GitHub
  actually sends was checked rather than assumed -
  `release-assets.githubusercontent.com` answers the final 200 with
  `Content-Length` and no `Transfer-Encoding` - so the live path is really
  length-checked rather than vacuously passing. A refused download takes the
  same path as an empty one, so the reader gets the cached copy and a status
  line that does not claim a fresh download.
- **A key sequence `decode()` would mangle is refused instead of sent.**
  `decode()` has no error return and every arm of its switch pushes a byte, so a
  mistake was not diagnosed, it was transmitted. `validateSequence()`'s refusals
  were read out of `decode`'s arms rather than guessed: a trailing backslash, an
  unknown escape letter, an octal escape over 377 (`val & 0xFF`, so `\400` is
  NUL - measured, and the case the function was written for), a trailing caret,
  and a caret on anything whose upper-case form is outside 0x40..0x5F. An empty
  string is accepted, because that is how a key is unbound. It is a second copy
  of `decode`'s arm structure and nothing in the language keeps the two
  together, so the suite ties them: for all three arms that can refuse, it
  decides what `validateSequence` *ought* to say by running `decode` and looking
  at the bytes - over all 256 successors of a backslash, all 256 of a caret, and
  every octal value from 000 to 777. On the page itself, validating per
  keystroke and *storing* per keystroke turned out to be different things, and
  the first version got that wrong in a way that was measured rather than
  reasoned about: typing an over-long octal escape left the row bound to a
  prefix of it, because every prefix raises its own `wxEVT_TEXT` and some
  prefixes are legal on their own, so a live commit cannot tell "finished" from
  "half typed". The box validates on every keystroke and commits only when the
  selection leaves the row or OK is pressed. And a refused sequence stayed
  refused on the status line after **Default** or **Unbind** had made it valid
  again, because the retraction lived in one of the five places that set the
  flag; the flag has a single writer now.
- **The rendering suite reddened one run in four, and one of its sections was
  asserting nothing.** Measured, not suspected: the same binary run twenty times
  failed five times, all twenty-two colour checks at once, because
  `PrintWindow(PW_RENDERFULLCONTENT)` asks the desktop compositor for the window
  and the compositor sometimes hands back a region it has not drawn into. A
  suite that reds a quarter of its runs teaches people to re-run it rather than
  read it. The capture is retried until the terminal's *own area* stops being
  one flat colour, and prints SKIP if it never composes; the area is the point,
  because the first attempt asked whether the whole window bitmap was uniform,
  which it never is - the frame and title bar compose before the client area
  does - so a blank terminal passed that check and failed everything after it.
  Thirty consecutive runs clean, against fifteen of twenty before. And the
  section called "no colour is drawn as its bit-reversed twin" skipped any index
  whose bit-reversal equalled its expected CGA value - but `ANSI_TO_CGA` *is*
  that bit reversal, so the guard was true for all eight and `check()` was never
  reached. Its comment said "0, 2, 5, 7 are their own mirror", which understated
  it by half. The guard is `i == ANSI_TO_CGA[i]` now, which really does skip
  exactly those four, and the other four assert that no pixel of the cell
  carries the untranslated value. Proved by sabotage: with `ansiToCGAColor`
  patched in a scratch copy to return its argument, the section fails four
  checks that it did not previously even print.

- **The Dazzler had three create-and-show paths and only one of them was right.**
  `todo.txt` carried this as three items and they were one bug with three faces.
  `DazzlerWindow::setScale()` sized the window from the card's *current* video
  mode - `updateSize()` used `m_dazzler->getWidth() * m_scale`, and `getWidth()`
  reads `m_x4Mode` and `m_use2K`, both false from `Dazzler`'s constructor, so on
  a card no guest has touched it is 32. A scale-4 window was sized to a 128x128
  client where `create()` had just given it 512x512. `create()`'s fixed
  `128 * scale` is the contract - it says so, and `paint()` `StretchDIBits`' the
  card's mode over the whole client rect - so `setScale` shrank a window that had
  been sized correctly, which is why it had ended up with **no caller at all**.
  Both sites name `Dazzler::MAX_WIDTH`/`MAX_HEIGHT` now rather than writing 128,
  so they cannot drift apart, and `updateSize()` no longer needs a card:
  `applyDazzlerState()` detaches the window from its `Dazzler` before rebuilding
  the card for a port change, and the old `!m_dazzler` early return made the
  scale silently not apply in exactly that case. Resizing in place also keeps the
  `HWND` and, through `SetWindowPos(SWP_NOMOVE)`, the position the user dragged
  the window to, which the rebuild had been reconstructing by hand.
  **Closing the window told nobody.** `WM_CLOSE` answered with `show(false)` -
  "Hide instead of destroy - let main window manage lifetime" - while
  `m_dazzlerEnabled` stayed true and *View > Dazzler* stayed ticked over a window
  that was not on screen, so getting it back cost two clicks. It posts
  `WM_APP_DAZZLER_CLOSED` to the handle `create()` stashes in `m_parent` - the
  only reader that member has ever had - and `MainWindow` routes it through
  `onViewDazzler()`: closing the window is the same gesture as unticking the menu
  item, so it takes the card down, clears the check mark and saves the choice.
  Posted rather than called back, and the thread is the point: both creators run
  on the UI thread, so the post lands on that thread's queue and the owner's
  handler runs after `WM_CLOSE` has returned, where a direct call would re-enter
  the `DazzlerWindow` still inside `handleMessage()` - what the owner does with
  the news is `show(false)` and `setDazzler(nullptr)` on that very object. The
  handler is guarded by `m_dazzlerEnabled` because `onViewDazzler()` *toggles*: a
  *View > Dazzler* click already queued ahead of the post would otherwise be
  undone by it.
  **`applyConfig()` carried a second copy of the code and it was the copy that
  got none of the fixes** - it left an existing card at its old port
  (`enableDazzler()` returns at once when one exists), left an existing window at
  its old scale, and `show(true)`'d unconditionally, so loading a profile
  reopened a Dazzler window the user had closed. It calls `applyDazzlerState()`
  now. Two things had to be checked before routing it, because `applyConfig()`
  runs at *startup* as well as on a profile load: it acts only when the config
  says enabled or when there is a card to take down, so a startup with no Dazzler
  never reaches the disable arm and never writes its status text or unticks the
  menu - exactly what the old code did - while on a profile load the same clause
  is a fix, since a profile with the Dazzler off used to leave a running card
  running with its window on screen and its menu item ticked, and the next save
  wrote that live state back over the profile. And `applyDazzlerState()` sets the
  status bar, which is right when the user just asked for the Dazzler and wrong
  from here, so the line it found is the line it leaves.
  Measured by driving the built app (`WM_COMMAND`, `GetMenuState`,
  `GetClientRect`, `PrintWindow`): scale 4 gives a 512x512 client; a Settings
  change to scale 2 leaves a 256x256 client, the same `HWND` and the same window
  position; the window's X unticks *View > Dazzler* and one click brings it back;
  a profile carrying port `0x1E` and scale 3 moves the live card and resizes the
  window to 384x384; a profile with the Dazzler off closes it; and three
  close/reopen cycles with the machine running leave it running.
- **Three controls in Settings were reading and writing nothing.** The Dazzler
  group was seeded from nothing and read back nowhere, so the checkbox opened
  *unchecked* on a machine running a Dazzler and a port typed into it was gone at
  OK. `todo.txt` had asked which of `cfg.dazzlers[0]` and the live card is
  authoritative when they disagree at startup, and the source answers it rather
  than a preference: at startup the **config** is, because it is the only side
  that exists - `m_dazzlerEnabled` starts false and `getDazzler()` starts null,
  and `applyConfig()` manufactures both from `cfg.dazzlers[0]`; afterwards the
  **live card** is, because `updateConfigFromState()` rewrites `cfg.dazzlers[0]`
  from it on every save. So the seed follows the rule the write-back already
  used. That answer is what made this more than two lines per side: a write into
  the config alone would have been discarded a second time by the
  `saveSettings()` at the bottom of the same function, so the machine has to
  change first - and changing the port of a running card means tearing it down,
  because `enableDazzler()` returns early when one exists and `Dazzler` has
  `getBasePort()` but no `setBasePort()`. That is `applyDazzlerState()`, which
  `onViewDazzler()` now calls too, so the menu toggle and the dialog cannot
  drift. `debugMode` was assigned false under a "TODO: get from emulator" and
  there is nothing to get - `EmulatorEngine::m_debug` is private with no getter -
  so `cfg.debug` is the only durable record, and the OK path was not writing it
  either. `setCacheRoot()` is the one line `HelpAssets.h` asked for, in
  `onCreate()`, and its placement is provable rather than probable: both routes
  to `ShowHelpWindow` are dispatched from `run()`'s message loop, while
  `onCreate` runs inside the `CreateWindowExW` that `create()` performs before
  `run()` is called.
- **A detached download worker outlived the dialog it was posting to, and that
  one had dumps behind it.** `SettingsDialogWx::~SettingsDialogWx` was empty and
  the dialog is a *stack* object - `ShowWxSettingsDialogInternal` builds
  `SettingsDialogWx dlg(nullptr, catalog)` and it dies the instant `ShowModal()`
  returns. The constructor starts a catalog fetch, and `DiskCatalog::fetchCatalog`
  runs its callback on a **detached** `std::thread`, so closing *Settings* before
  the fetch landed left that worker calling `wxPostEvent` on freed memory. Every
  dump it produced was `0xC0000005` at the same address, `z80cpmw.exe+0x5ABF3`,
  with the faulting stack running thread trampoline into the fetch worker into
  the `std::function` call. The fix is a gate the workers hold instead of a bare
  pointer: a mutex and a bool, built in the constructor's init list because the
  body hands a copy to a thread before the constructor has finished.
  `postIfOpen()` takes the mutex *across* the `wxPostEvent`, the destructor's
  `close()` takes the same mutex, and that is the whole of it - once `close()`
  has returned no worker can post, and a post already begun finished while the
  dialog was still whole. A `weak_ptr` to the dialog was tried on paper and
  rejected: locking it and finding the dialog alive still lets the UI thread
  return from `ShowModal()` and destroy it before the `wxPostEvent`, because the
  `shared_ptr` keeps the *control block* alive, not the `wxDialog` - nothing in
  that scheme ever makes the destructor and the post exclude each other.
  `onDownloadDisk` got the same gate unasked, being the same shape and the worse
  of the two, since its progress callback fires once per read block for a
  multi-megabyte image rather than once at the end. The calls stayed
  `wxPostEvent` rather than becoming `wxQueueEvent`, and that was checked rather
  than assumed: `wx/event.h` does warn that `wxPostEvent` is not thread-safe
  because `Clone()` shallow-copies `wxString`, but `wx/string.h` typedefs
  `wxStringImpl` to `std::wstring` unconditionally and MSVC's copy constructor
  deep-copies, so the hazard is not present in this build. Measured with the
  driver that found it - `WM_COMMAND 2004`, then `WM_CLOSE` after a delay, twelve
  cycles a run. Before: dumps at 0, 150, 400 and 800ms, the delay moving only the
  odds. After: 144 open-close cycles over 0/50/150/250/400/600/800ms with
  repeats, zero dumps, and a dialog left open still reaches "Catalog loaded" with
  the catalog's twenty entries.
- **`DiskCatalog` was destroyed out from under its own workers, and the debug
  build was the only one that looked well behaved.** The workers run detached and
  nothing joined them, so `MainWindow`'s `unique_ptr` could free the object while
  one was still reading `m_cancelRequested` and `m_downloadState` down the
  download path, `m_catalogMutex` and `m_catalogEntries` down the fetch path, and
  `m_downloadDir` down both - out of the freed block. It is a `shared_ptr` now
  and each worker captures `shared_from_this()`, so the last reference out
  destroys the object, on whichever thread that is.
  Measured with a 4s sleep inserted after the release, because the natural window
  is the ~45ms between `WM_CLOSE` and `ExitProcess`: 34 natural shutdown cycles
  with a fetch in flight and 10 with a 49MB download in flight produced **no dump
  at all**, before or after, and that is reported rather than dressed up.
  Widened, the shipping code did three different wrong things and not one of them
  was a crash. With a download running the read loop read `m_cancelRequested`
  back as raw byte `0xDD` - the debug CRT's freed-memory fill - within 0-78ms of
  the release and took it for a cancel the user never asked for. With a fetch
  running the worker went into `std::mutex::lock` on the freed `m_catalogMutex`
  and never came out, 3/3. And rebuilt `/MD`, which is what ships, `free()` does
  not fill: the same read came back **false** in 3/3 runs, nothing bailed out,
  and the whole tail ran to completion - reading a download directory back as a
  15-character string where a 45-character path went in. The debug build looked
  well behaved only because `0xDD` happens to mean true.
  Joining in `~DiskCatalog` was rejected on what the code does: the worker sits
  in WinHTTP and nothing in that file calls `WinHttpSetTimeouts`, so a join is
  bounded only by WinHTTP's defaults with the UI thread stopped in it. Shared
  ownership costs one atomic and blocks nothing. `~DiskCatalog` can now run on a
  worker thread, which is safe because of what it does - a string, a vector, a
  mutex and two atomics destroyed, no window handle, no wx, no COM - and it
  cannot race the worker that runs it, since a worker's `shared_ptr` lives in the
  lambda's captures and the trampoline destroys those only after the body has
  returned.
  Its `cancelDownload()` is gone, being unreachable-with-effect for exactly the
  reason shared ownership exists, and the one useful thing that call was doing by
  accident is explicit now: `MainWindow::onDestroy` cancels. At quit the transfer
  cannot finish, and the read loop noticing is what gets `downloadToFile` to its
  cleanup label, closes the file and removes the partial `.img` - which
  `diskFileLooksComplete`'s 1MB floor would otherwise take for a finished
  download and boot the guest from. `onDestroy` rather than `~MainWindow` buys
  the worker the whole message-loop drain to act on it. Confirmed: the data
  folder is empty after a run of shutdowns mid-transfer.
  **The third caller is why `shared_ptr` alone was not the fix.**
  `downloadAndStartWithDefaults`' completion callbacks captured `MainWindow`'s
  raw `this` and called `runOnUiThread`, which reads `this->m_hwnd` on the worker
  thread - and `MainWindow` is a stack object in `wWinMain`. Keeping the catalog
  alive *longer* would have made that path strictly more reachable, not less: the
  trace caught the worker inside `MainWindow::runOnUiThread` while the UI thread
  was inside `~MainWindow`, printing `isWindow=0`. `runOnUiThread` is replaced by
  a file-static `postToUiThread` taking the window and a gate by value, so the
  worker touches no `MainWindow` state at all. Capturing the `HWND` alone was not
  enough - handles are recycled, `IsWindow`'s own documentation warns of it, and
  a `WM_APP_RUN_ON_UI` carrying a heap pointer must not be delivered to somebody
  else's window - so `onDestroy` closes the gate and after that no post happens.
  The dialog gate above is the same object, promoted to `WorkerPostGate` in
  `DiskCatalog.h` next to the callback contract that creates the need.

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
- **All 24 `Write-Error` sites in the packaging scripts now reach their
  `exit 1`. NOT EXECUTED.** Both scripts set `$ErrorActionPreference = "Stop"`,
  which makes `Write-Error` *terminating*, so the `exit 1` written under every
  one of them was dead code and the exit code was left to the host. The count
  was re-derived rather than taken on the earlier note's word: 15 in
  `build-msix.ps1`, 9 in `build-nsis.ps1`, and each one is followed by an
  `exit 1` (the one other `exit 1`, at the NSIS-not-found branch, follows
  `Write-Host` and was always reachable). Each site gained `-ErrorAction
  Continue`, which overrides the preference variable for that call only, so the
  message still goes to the error stream and the `exit 1` under it runs. The
  visible behaviour is unchanged — loud, non-zero, no package claimed as good —
  and what changes is that the exit code is now the script's own rather than
  whatever the host makes of an unhandled terminating error. A comment beside
  each `$ErrorActionPreference` line says so, so the next failure site is
  written the same way. **Nothing here can run PowerShell**, not even `-WhatIf`
  - there is no `pwsh` on the machine this was written on - so that `-ErrorAction`
  overrides `$ErrorActionPreference` for the call it is on is documented
  behaviour taken on trust, not behaviour anyone watched. It is the first thing
  to check when one of these scripts next fails on purpose.
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
- **`build-msix.ps1 -Beta` keeps the build's symbols.** There is no `.pdb` for
  the shipped 1.0.22 on either channel, and there cannot be one: a rebuild is a
  different binary, so a crash report from that build can never be resolved. The
  `-Beta` branch now copies `bin\<Configuration>\z80cpmw.pdb` out to
  `dist\z80cpmw-<ver>-beta.pdb` after the package is signed, and *fails* if it is
  not there rather than warning, because silence is exactly how the 1.0.22
  symbols were lost. **This has not been run.** It was written on a machine with
  no PowerShell, no MSVC and no Windows; nothing here can execute it, not even
  `-WhatIf`. `todo.txt` keeps the item open until someone runs
  `build-msix.ps1 -Beta` on a Windows machine and sees the `.pdb` land in
  `dist\`. The default (Store) branch and `build-nsis.ps1` still keep no
  symbols, which matters only when the two channels do not come off one build
  the way 1.0.22 did.
  **Correction, 2026-08-28.** It has been run, and the instruction above is now
  the *forbidden* run. Because `-Beta` reached the signing call before anything
  looked at `-SkipSign`, the only way to obey that sentence on 1.0.22 was to
  spend a real signing call and write `dist\z80cpmw-1.0.22-beta.msix` - the
  name 1.0.22 is already published under - over the published artifact, which is
  what happened; see the `-SkipSign` entry below. What to run instead is the
  unsigned rehearsal, `build-msix.ps1 -Beta -SkipBuild -SkipSign`, which exits
  0, writes `dist\z80cpmw-1.0.22-beta-unsigned.msix` and its `.pdb`, and leaves
  the published package untouched. A **signed** `-Beta` run stays forbidden
  while `Version.h` says 1.0.22, and `todo.txt` keeps the item open on those
  terms rather than on the sentence above. The symbols themselves are not
  recovered and cannot be:
  `dist\` holds no `z80cpmw-1.0.22-beta.pdb`, a rebuild is a different binary,
  and a crash report from the shipped 1.0.22 stays unresolvable. What the change
  buys is 1.0.23 onward.

- **`tools/check-sibling-drift.sh` measures against `origin`, not against the
  local checkout.** Comparing a recorded reading with a sibling's local `HEAD`
  lets a stale checkout certify a column as current, and it did: on 2026-08-27
  the `ioscpm` line read "current" while the checkout on this machine was two
  commits behind `origin/main`, so the column was being blessed against a tree
  nobody else had. The tip compared against is now `refs/remotes/origin/HEAD`,
  falling back to `origin/main` and then `origin/master`; a local `HEAD` behind
  that tip is reported in its own right, because a reading taken in that
  checkout was taken against the wrong source; and a checkout with no origin ref
  at all now fails loudly rather than being quietly measured against itself — a
  gate that cannot verify must not say yes.
  It still does not fetch. A remote-tracking ref is only as fresh as the last
  `git fetch` in that tree, so every line prints when that was rather than
  leaving the age to be assumed, and `--fetch` updates them first — the only
  thing the script does that writes to a sibling, which is why it is off by
  default.
- **`build-msix.ps1 -Beta` honours `-SkipSign`, and `-WhatIf` is a binding error
  rather than a word the script ignores.** The signing test was
  `if ($Beta) { ... } elseif (!$SkipSign -and $CertificatePath) { ... }`, so
  `-Beta` reached the Azure Trusted Signing call before anything looked at
  `$SkipSign`: the one switch whose whole purpose is to keep a run off the
  network was overridden by the one branch that goes to the network, and the
  beta vehicle therefore had no dry run at all. Nothing would have reported
  that. The signing kit is complete at the hardcoded default
  `C:\temp\in\z80cpmw-signing-kit` and `$env:Z80CPMW_SIGNING_KIT` is unset, so
  the "Signing kit not found" guard does not fire; `Assert-ExeVersion` compares
  `bin\Release\z80cpmw.exe` against `Version.h` and says nothing about the
  artifact being replaced; and both binaries carry 1.0.22.0 while differing in
  size - 607,744 bytes in `bin\Release` against 605,184 inside the published
  package - so the guard passes while two different builds wear one version
  number. The branch is three arms now, `$SkipSign` checked first on each, and
  the unsigned outputs are named from a single `$betaStem` carrying
  `-unsigned`, so the `.msix` and its `.pdb` cannot be named by two expressions
  and drift apart, and the only file the "remove existing package" step can
  delete is a previous rehearsal's. The marker is in the file name rather than
  only in the console output, because the file outlives the console: a beta is
  identified by its name when it is attached to a release. Step 6's symbol copy
  and its `Write-Error` run on both beta arms, since that copy is the thing a
  rehearsal exists to check.
  `[CmdletBinding()]` is added for what it refuses, not for the common
  parameters it brings, **and this was measured expensively.** A `param()` block
  without it makes a *simple* script, and PowerShell quietly collects unmatched
  arguments into `$args` instead of failing. `-Beta -SkipBuild -SkipSign
  -WhatIf` was run against the old script on the expectation that `-WhatIf`
  would be rejected; instead `-WhatIf` fell into `$args`, the run proceeded for
  real, made a live Trusted Signing call, and re-minted the published 1.0.22
  beta package from a different binary. The GitHub release was never touched and
  the local copy was restored from it - its published digest `1549c223...`
  matches `dist\` again - but the signing call is spent. That is the reason the
  guard exists, and it is recorded here rather than left in a commit message.
  `SupportsShouldProcess` is deliberately not taken: a token `-WhatIf` that
  skipped only the steps someone remembered to guard would be a worse lie than
  none. `packaging/scripts/build-nsis.ps1` has the same missing
  `[CmdletBinding()]` and still swallows a stray `-WhatIf`; it is left for its
  own commit, and `todo.txt` and `STORE_SUBMISSION.md` both name the gap.

- **`tools/check-sibling-drift.sh` stopped measuring only commits.** Counting
  commits answers "has this tree moved", which is not the question
  `FEATURE_PARITY.md` exists to answer. Two things were added, neither visible to
  a commit count, and both came out of what re-reading the columns turned up.
  **Shipped build.** The `sibling-readings` line carries `shipped:<build>`, and
  the script reads the build out of each port's tree at the recorded sha -
  `CURRENT_PROJECT_VERSION` from `ioscpm`'s pbxproj, `versionCode` from
  `cpmdroid`'s `build.gradle.kts`, `romwbw_emu`'s `VERSION` file, the four
  `#define`s in this repository's `Version.h` - and says so when they differ from
  what is served. A tick means "in the tree", which is not what a user has:
  `ioscpm`'s row 2 was a tick from 2026-07-25 over scrollback that had never
  captured a line, and after that was fixed the App Store still served build 37
  against a tree at 57. `shipped:unknown` **fails** rather than passes.
  **Citations.** Prose about a sibling is delimited with `<!-- cites: repo -->`
  and every backticked identifier inside must resolve, by `git grep`, in that
  port at the recorded sha. The Android block cited nine symbols that existed
  nowhere in `cpmdroid`; this is what catches them. Three refinements were earned
  by running it rather than by designing it: the greps exclude `*.md`, `*.txt`
  and `docs/`, because `cpmdroid`'s own `todo.txt` now quotes all nine fabricated
  names in the course of recording that they were fabricated, and an unscoped
  grep finds them there and calls them real - a document cannot be evidence for
  itself. Absent at the recorded sha is two different things: present at `origin`
  means the **reading** is stale and the symbol arrived later, and only
  absent-everywhere is fabrication; calling those by one name is how a checker
  earns a reputation for noise. And `cites-withdrawn:` declares symbols the
  document names *in order to record that they never existed*, inverting the test
  so it fails if one ever resolves - which makes naming the nine a live assertion
  rather than a comment that rots. `cites-elsewhere:` declares deliberate
  cross-port references, so the exception is visible in the document rather than
  silent in the checker.
- **A matching version number is not a matching build, and the check says so.**
  Recording `cpmdroid`'s shipped `versionCode` 25 turned up something the check
  was getting wrong: numbers matching is weaker than it looks. A version is
  bumped when somebody cuts a release, so everything landing afterwards sits in
  the tree under the previous build's number. `cpmdroid`'s tree read
  `versionCode` 25 against a Play console serving `versionCode` 25 with four
  commits between them, two of which were the scrollback fixes no user had, and
  the check would have called that a clean pass. It now reports how far the tree
  has moved since the number was set, and fails when it has. It prefers a
  **release tag** where one exists, because a tag names the commit an artefact
  was cut from where the bump commit only names when the number changed -
  `romwbw_emu` bumps `VERSION` and tags a commit later, so measuring from the
  bump said two and the tag says one, which is the true answer. That immediately
  reclassified `romwbw_emu`, which `FEATURE_PARITY.md` had recorded as "build
  1.38, and that is what ships" an hour earlier; it is one commit past the tag,
  and that claim was as overstated as the ones this document has spent a week
  correcting - made by the check rather than by a person, which is not a defence.
  Two fixes were needed to get there, both from ports disagreeing about what they
  tag: the lookup tried only the build *number*, and `cpmdroid` ships
  `versionCode` 25 under the name 1.24 and tags the name, so `v1.24` was
  invisible and it silently fell back to the bump commit while printing the
  message for a tag. `tree_release_name()` reads `MARKETING_VERSION`,
  `versionName` or `VERSION`, and tags are tried by both. The fallback also
  returned a bare sha where the caller expected `<sha> <label>`, so the output
  named a 40-character hex string as the release it measured from. Both paths are
  exercised now, and the message distinguishes them, because "four commits after
  `v1.24`" and "four commits after the number was set" are different claims - the
  first is the artefact, the second an upper bound a tag would sharpen.
- **Drift is classified by what a commit touches.** A column reading is a reading
  of *code*, so documentation landing after it does not invalidate it, and
  reporting that at the same weight as a real change is how a checker gets
  ignored. `ioscpm` was failing on a `todo.txt` commit and `romwbw_emu` on a
  changelog one; both read current now, with the count stated. The three that
  still fail are the three carrying code users do not have.
- **`z80cpmw` has a `sibling-readings` line of its own.** This repository was
  absent from that block because it describes itself, so the shipped-build check
  never ran on the one column every other column is scored against - the same
  asymmetry the 2026-09-02 audit found in the prose, reproduced in the mechanism
  and pointing the other way. The home repo has no reading to drift, since its
  column is edited in the same commit as the code it describes, so only the
  shipped question is asked. The self-description was stale in the way this whole
  exercise has been about: "the attribute work, and the bright half of the
  palette ... are unreleased" named two things and was true on 2026-08-28, and
  twenty-three source commits later it read as a complete list and was not one.
  Replaced with the count and the shape rather than a list that will rot again.

### Documentation
The cross-port sweep that produced `FEATURE_PARITY.md` was committed the same
afternoon as several of the upstream commits it describes, so parts of it were
stale within the hour. These are the corrections that have landed. The rows that
were still carrying the old text in their own paragraphs were swept on
2026-08-26; `todo.txt` no longer lists any.
- **Row 12 no longer says the web frontend forgets *Don't warn*.** `romwbw_emu`
  `108856c` moved the suppression call outside the `diskData` guard in
  `reloadDisks()`, so the checkbox survives a mid-session disk reload. This row
  had no stale paragraph of its own, so it is corrected outright.
- **Row 4 and row 13 are marked fixed upstream** in the header summary and in
  the per-port snapshot table: `98eb6a1` gave the `romwbw_emu` CLI
  `W8 <cpmname> [hostpath]`, and `2dbf6f2` opened `Module.onConsoleOutput` to
  every byte and implemented `Module.onError`. Each row's own paragraph still
  described the old behaviour at that point; both were rewritten on 2026-08-26,
  below.
- **The Android column is rewritten from source, and the `c26aeb7` dispute is
  settled.** `cpmdroid` was checked out on this machine for the first time, so
  the column could finally be read against the repository rather than against a
  commit nobody could find. The branch is not coming: `c26aeb7` is not a valid
  object there, and none of the symbols the column cited — `DEFAULT_KEY_BINDINGS`,
  `decodeKeySequence`, `saveProfile`, `showFileTransferDialog`,
  `res/xml/file_paths.xml`, VT52, DECSTBM — exist at `origin/master`. Five rows
  were overstated and three of them go from ✅ to ⬜: **1** (there is no key map
  on Android at all), **4** (no Files button, no SAF import picker, no share
  sheet — no UI for either transfer folder, which makes Android the *worst* cell
  in that row rather than level with iOS), **6** (help is fetched from
  `releases/latest` with nothing bundled — the exact trap the row exists to warn
  about, described as fixed), **11** (no profiles; a flat `SharedPreferences`),
  and **13** (the CSI dispatch is `H f A B C D J K m` and nothing else). Row 13's
  lead paragraph credited `cpmdroid` with extending `ioscpm`'s parser; it has the
  thinnest parser of the four ports.
  What the rows shared is worth recording: each named specific symbols and file
  paths, which reads as evidence, for code that was never pushed. A citation is
  not a reading. Every Android cell now names something that can be grepped for.
  Some of the column moved the same day for a better reason — `cpmdroid` took the
  v1.36 core and closed four gaps this document names (F1–F12, the Ctrl window,
  scrollback as a setting, TAB) — and those are marked where they land.
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
  file: it is 252, not the 205 both claimed - and 252 is not taken on the earlier
  entry's word, it is what counting `CHECK_INT(` / `CHECK_STR(` / `CHECK_TRUE(`
  in `tests/test_vt52.cpp` gives (255, less the three macro definitions), which
  is the same number the suite reported when it was run.
- **`FEATURE_PARITY.md` records which commit each column was read at**, in a
  `sibling-readings` block that `tools/check-sibling-drift.sh` reads. The
  `romwbw_emu` line says `unknown`, because the 2026-08-24 sweep never wrote
  down what it read and it cannot be recovered — recording a plausible guess
  there would be the same habit that produced the `c26aeb7` citations, so it
  stays `unknown` until someone re-reads that column. The two `9b68ab1` cites
  in the Android text were re-grepped at `c06fa58` and hold; the block still
  names `9b68ab1`, since a re-grep of two claims is not a re-read of a column.
- **The lesson from the three claims that outlived the Android rewrite is now
  in `FEATURE_PARITY.md` rather than in `todo.txt`.** A sweep of a column is not
  a sweep of the document: the three that survived were in *Suggested priority
  order* and in a row bullet, neither of which is a row. Grep the whole file for
  the port's name afterwards.
- **The shipped `[1.0.14]` entry is corrected in place.** It claimed "R8 / W8
  host transfers now honor absolute paths", and that is where the same claim in
  `README.md` and in `HelpWindow.cpp` came from. `W8` took no host path until
  `romwbw_emu` `98eb6a1` on 2026-08-24, two and a half months after that
  release; `89a4f28` changed the *backend*, which only `R8` ever reached. The
  entry keeps a dated correction beside it rather than being edited away - the
  same treatment `cpmemu` `b821ec0` gave its own bad `DONE` claims - and the
  correction also records that the line's second half was early rather than
  invented: at 1.0.14 only **Settings** showed the resolved data folder, while
  About still showed the un-redirected `%LocalAppData%\z80cpmw` and the boot
  banner printed a literal `%LOCALAPPDATA%\Packages\<package>\...` template.
  `1f44e20` put the real path in both and shipped in `[1.0.15]`, whose own entry
  records only the About half. `README.md` and `HelpWindow.cpp` are deliberately
  left alone; they come right when the disk images are refreshed.
- **`docs/FILE_TRANSFER.md` no longer promises a `W8` that takes a host path.**
  Its headline example, `W8 C:\Users\me\Desktop\out.com`, described the `w8.com`
  `98eb6a1` introduced, not the one inside the disk images this app ships —
  which reads only the default FCB and writes into the data folder whatever you
  type. The quick-reference table and the Windows section now say so, and say
  what to do until the images are refreshed. The same claim survives elsewhere
  in `README.md` and the in-app help; `todo.txt` lists where.
- **Row 4's `romwbw_emu` CLI paragraph is rewritten.** It still said `W8` "takes
  no path at all" and wrote into the emulator's CWD, a hundred lines below a
  summary saying the row was good again, so the file contradicted itself for
  whoever reached the row without reading the header. Checked against
  `romwbw_emu` `src/w8.asm` and `src/emu_io_cli.cc`: `W8 <cpmname> [hostpath]`,
  the whole rest of the line taken as the path so a directory may contain
  spaces, `resolve_write_path` resolving the parent case-insensitively and
  lowercasing the leaf, `emu_host_file_get_write_name()` answering with the
  absolute path that was opened, and the `0xE9` capability guard that makes it
  refuse a path to a backend that will not promise it is safe. The CLI's own
  `--help` describes all of it now, where it used to say "Export CP/M file to
  emulator CWD".
- **Row 13's web paragraph and row 10's `Module.onError` sentence are
  corrected.** `2dbf6f2` fixed both and the sweep predated it by hours. The web
  output filter no longer starves xterm.js - every byte reaches `term.write()`,
  with LF the one exception, written as CR LF - and `Module.onError` is
  implemented, to the status line and to `console.error`. The rest of the
  Dazzler paragraph stands and is deliberately kept: the C++ side still emits
  `Module.onVideo*` / `Module.onDsky*` while the page implements `Module.onVda*`
  / `Module.onSnd*`, zero overlap, and that same commit looked at the dead
  wiring and left it. Worth keeping the connection the fix makes explicit: the
  channel that would have complained about the dead wiring was itself unplugged.
- **The iOS/macOS column is re-read from source, at build 52.** It was dated at
  build 50, and build 51 (`4deea96`) landed the same day; `ioscpm` is checked out
  on this machine, which `todo.txt` denied, so the column no longer stands on a
  reading nobody here can repeat. Build 51 closed the three complaints this
  document had left against that port, and each row now says so rather than only
  the header: **1** (`SpecialKey` is twenty-two cases carrying this repo's own
  VT220 bytes, not ten with no F-keys - the row stays ◐ for the modifier
  bindings, the key names and the absent on-screen key row, not for its size),
  **6** (the index and all seven topics ship in the app, behind the download and
  the cache, referenced in place from `release_assets/` so there is no second
  copy to drift), and **13** (`@ P X S T` all implemented, DECAWM and DECTCEM
  acted on rather than parsed and dropped). Build 52 (`bb5543f`) is recorded
  under row 4 because it is this family's own hazard: `98eb6a1` gave `W8` a host
  path, that port stored it unsanitised, and `W8 ANYFILE.TXT ..` deleted the
  user's whole `Documents` folder - every downloaded disk image - while
  reporting success to the guest. That is what the release order in
  `romwbw_emu` `docs/RELEASE_ORDER_2026-08-25.md` exists to sequence around.
- **Three `c26aeb7`-era claims survived the Android column rewrite.** `944cf9f`
  swept the rows; two of these sat in the "Suggested priority order" list, which
  is not a row, and the third in a bullet of row 4 rather than in that row's
  per-port paragraphs. Priority #2 named `buildKeyRow` in `MainActivity.kt` and
  `TerminalView.sendNamedKey` as "the worked example" for an on-screen key row -
  neither symbol is in `cpmdroid` at `9b68ab1`, whose `setupControlStrip` is
  Ctrl, Esc, Tab, Copy and Paste. Priority #4 said arbitrary-path R8/W8 was "now
  only the export half", which reads as Android already having an import picker;
  it has neither half - no picker, no `ACTION_CREATE_DOCUMENT`, no path shown
  anywhere. And row 4's parity-targets bullet said findability was "done on
  Android (share sheet plus the paths shown in-app)" when that tree has no
  `ACTION_SEND`, no `FileProvider`, no `res/xml` and no path display at all. All
  three corrected; `todo.txt` records the lesson, which is that sweeping a column
  is not sweeping the document.
- **`todo.txt` and `FEATURE_PARITY.md` cite symbols instead of line numbers now,
  and say why.** The cross-port sweep section of `todo.txt` carried eight
  distinct `file:line` cites. Six pointed into `FEATURE_PARITY.md` - this repo's
  own file, not a sibling's - and five of those six were invalidated before
  anyone acted on them, by this repo's own next two commits, inside the same day.
  They were written in `db5392b` at 03:59 on 2026-08-25; `2f10d4c` broke `:439`,
  `:579` and `:606` at 14:33, and `944cf9f` broke `:32` and `:53` thirty-five
  minutes after that. At HEAD they read `:32` to `:37`, `:53` to `:58`, `:439` to
  `:449`, `:579` to `:604`, `:606` to `:633`. The other three had not drifted at
  all: `FEATURE_PARITY.md:261` still lands on the paragraph it named, and
  `README.md:98-102` and `HelpWindow.cpp:57` on the `W8` claim and the "Give a
  full path (recommended)" line. Nor had the three cites elsewhere in `todo.txt`
  pointing at this repo's own source - `TerminalView.h:20`,
  `TerminalView.cpp:602-607` and `:616` - which were rewritten for consistency,
  not because they had rotted. That split is the argument rather than an
  exception to it: five of eight went stale inside twelve hours of being written
  and nothing on the page said which five, so a line number reads as evidence
  while carrying none.
  Eleven cite lines in `FEATURE_PARITY.md` and four in `todo.txt` are now a
  function name, a symbol or a greppable string, and the fifth was retired with
  the paragraph it sat in.
  `grep -nE '\.(md|cpp|h|kt|swift|cc|txt|html-template):[0-9]+'` returned 11
  lines in `FEATURE_PARITY.md` and 5 in `todo.txt` at `944cf9f`; it now returns
  nothing in `FEATURE_PARITY.md`, and in `todo.txt` only the four lines of that
  paragraph itself, which record which cites rotted rather than ask anyone to
  follow one.

- **`FEATURE_PARITY.md`'s three sibling readings are all recorded and all
  current**, and `romwbw_emu` has one for the first time. `ioscpm` moves to
  `15f48e9`: row 4 gains the two facts that commit adds — `emu_io_ios.mm` now
  defines `emu_host_file_get_read_name()` and returns `""`, and the zero-byte
  `W8` export bug is fixed there in both halves (`close_write` no longer needs a
  non-empty buffer, and the Swift guard no longer gates on the write-data
  pointer, which by the shared contract is `nullptr` for an empty buffer and so
  could never answer the question). Nothing in that column was falsified.
  `cpmdroid` moves to `c6756af`: `c06fa58` closed the identical zero-byte export
  divergence in three places, and every absence claim in the Android column was
  re-checked at `c6756af` and every one stands.
- **Row 5's `romwbw_emu` cell was understated, and is rewritten from the
  source.** It said "hardcoded list, unpinned; 4 of 5 images ship nowhere". Read
  at `a95db9f`: `web/romwbw.html-template`'s two disk `<select>`s are five
  hardcoded names fetched by bare relative URL beside the page; `web/makefile`'s
  two deploy targets copy the page, the wasm and `vendor/` and **no image at
  all**; `.github/workflows/release.yml`'s staging step copies the page, the
  wasm, `vendor/`, `roms/*.rom` and `roms/emu_avw.rom` beside the page, and **no
  image at all**; and `web/` itself carries none. So it is five of five, in every
  vehicle — deb, rpm, either deploy, and `make serve` out of the tree — and both
  selects come up preselected, so it is what a first-time visitor gets rather
  than something they must go looking for. The cell moves from ◐ to ⬜: there is
  no catalog to be partial about, and "unpinned" is beside the point when there
  is no remote to pin to. The rest of that column was re-read row by row and
  twelve of thirteen cells stood.
- **`packaging/STORE_SUBMISSION.md`'s update flow gained a disk-image step**,
  between the Release rebuild and the package build, naming
  `verify-disk-assets.sh` and carrying today's measurement: the images shipped
  in 1.0.22 fail it.

- **`todo.txt` cut from 377 lines to 162, and almost none of what went was open
  work.** The file had stopped being a list of things to do and become a record
  of things considered: closing an item produced a paragraph explaining that it
  closed, which was reliably longer than the item had been.
  Nine items carried a paragraph opening "Re-checked 2026-08-26", "Left alone on
  2026-08-26" or "deliberately not written blind", and those paragraphs alone
  were 108 lines. Every claim in the file was re-verified against source before
  anything was deleted. What went:
  - **Six closed items, deleted rather than narrated.** Three of them were
    `cpmemu`'s two Windows console bugs and its missing harness coverage, all
    fixed in `cpmemu` `6daca11` the same afternoon this file was last edited —
    `read_one_key()` reads `INPUT_RECORD`s now and `_getch()`/`_kbhit()` survive
    only in comments explaining why they went, `SetConsoleCtrlHandler` sits
    beside the `atexit(disable_raw_mode)`, and `inject_spec()` has both a
    `U+XXXX` form and a `GenerateConsoleCtrlEvent` case.
  - **The 0-byte disk-image item, which was false when it was written and false
    again when it was re-checked.** `HBIOSDispatch::loadDisk` has refused
    anything under `EMU_MIN_DISK_SIZE` (512) since `romwbw_emu` `6e1f134`, which
    is an ancestor of the very commit the 2026-08-26 re-check measured against.
    The re-check inspected `emu_file_load`, which was never what decided the
    outcome, and then wrote fourteen lines arguing that the fix could not be made
    in this repository — a fix that had already been made upstream a day
    earlier. This port reaches the guard: `EmulatorEngine::loadDisk` returns
    false and `MainWindow` puts up "Failed to load disk image".
  - **The 29-line "CITE A SYMBOL, NOT A LINE NUMBER" preamble**, and the four
    self-referential cite lines it existed to explain. The rule survives as one
    line in the file header. The argument for it had already won; what was left
    was the transcript of winning it.
  - **The `cpmemu` items filed in this repository at all.** Five of the first six
    were `cpmemu`'s work, one a verbatim duplicate of `cpmemu/todo.txt`'s own
    first bullet, and one carrying a test count (`59 passed / 4 skipped`) that
    `cpmemu` had already superseded with `76 passed / 4 skipped`. Two files
    holding the same item is a guarantee they will drift, and they had. One
    pointer line replaces all five.
  - **The erase-fill cross-port question**, settled 2026-08-27 in this port's
    favour: an erase blanks with the current SGR background. Nothing changes
    here, so nothing is open here; the work is `ioscpm`'s and the decision is
    recorded in `FEATURE_PARITY.md` row 13 in place of the open question.
  Every surviving item now carries a tag — `[WINDOWS]`, `[RELEASE]`,
  `[DECISION]` — so a session on a machine that is not this one can see at a
  glance what that machine can take.
- **`MANUAL_CHECKS.md` (new).** The checks that need a person: file transfer
  under an **installed** MSIX, which is the only thing that reproduces
  file-system redirection, and the hands-on pass over keystroke delivery, mouse
  copy/paste rendering and the first-run Help window. Written as a checklist —
  what to do, in what order, what right looks like — with the standing rule that
  a check is *deleted* once someone runs it and its result goes under
  **Verified** here. `WIP.md`'s "Not verified on hardware" section moved into it
  whole rather than being copied, so there is one list and not two, and it gained
  a step for the new `emu_host_file_get_read_name()`: `R8` should report the
  resolved `LocalCache` path, not the name the user typed.
- **`KNOWN_PROBLEMS.md` (new).** Standing facts that will never be "done", which
  is why they were making `todo.txt` longer every round without ever leaving it:
  that 1.0.22 has no symbols and never will; that `emu_host_path_basename()` is
  declared in `emu_io.h` but defined only in `emu_io_common.cc`, the one core
  file this project deliberately does not compile, so the first call added here
  links against nothing; and that cpmtools with the wrong diskdef exits 0 in two
  different disguises, measured today: `cpmls -f wbw_hd1k hd1k_combo.img` prints
  1024 blank lines and not one filename, while `cpmls -f wbw_hd1k_0
  hd1k_infocom.img` prints 312 garbage names, 233 of which are not printable
  ASCII. Neither looks like an error, and the first is indistinguishable from
  "that utility is not on the image".
- **`README.md` stopped promising `W8` a host path.** Its "File Transfer
  (R8 / W8)" section said "give a **full path** and the file goes exactly there
  (even on the Store build)" over a `W8 C:\Users\me\Desktop\out.com` example.
  `W8` took no host path until `romwbw_emu` `98eb6a1`, and the `W8.COM` on the
  bundled images predates that: it reads only the parsed FCB and never the
  command tail, so every export lands in the data folder whatever it is handed.
  The example is now the `R8` one, which is true today, and the section names
  the two live hazards of the utilities that actually ship — the old `R8` hands
  an unfiltered host basename to `F_DELETE`, so importing a host file whose name
  contains `?` or `*` erases every matching CP/M file silently, and the old `W8`
  truncates a binary export at the first `1Ah`. Both come out when the images are
  refreshed, and `todo.txt` says so. The same claim survives in
  `HelpWindow.cpp`'s help topic, which is a string in a `.cpp` and stays open.
- **`WIP.md` corrected.** It claimed a clean build against `romwbw_emu`
  `2dbf6f2`, fourteen commits back and older than both the v1.36 sync and the
  link break above; it said `emu_io_windows.cpp` carries "the eleven functions"
  of `emu_io_common.cc` when `emu_rename` made it twelve; and it closed with a
  dormant `rename()` bug it said was upstream's to fix, which upstream has fixed
  — `emu_io_common.cc` grew its own `emu_rename()` using `MoveFileExA` with
  `MOVEFILE_REPLACE_EXISTING`.
- **The in-app File Transfer topic stops endorsing a path `W8` will not take**,
  which closes the "survives in `HelpWindow.cpp`" note two entries above.
  **Help > Help Topics > Getting Started** listed R8 and W8 together and then
  said "Give a full path (recommended)", which reads as a recommendation for
  both and is true of R8 only. The `w8.com` in the images this build ships
  prints `Usage: W8 <cpmname>` with no `[hostpath]` and carries none of the
  `06 E9 CF` bytes of the `HBF_HOST_CAPS` probe - measured over
  `bin/Release/disks`, not inferred from the lineage - so it reads only the FCB
  the CCP has already parsed, and every export lands in the data folder whatever
  the user types. `README.md` lost the same claim earlier; this one lives in a
  `.cpp` and needs a build to see, which is why it outlived the document. The
  replacement is `README.md`'s wording rather than new prose, so the two cannot
  drift apart while they wait. The two cautions stay, and one fact neither
  document carried is added, because the topic four lines above tells the reader
  to download the Games disk: `hd1k_games.img` has no `r8.com` or `w8.com`
  directory entry at all, so nothing transfers from it. A comment above the
  literal names the blocks that are *deleted* rather than reworded when
  refreshed images land, and each block carries its own condition -
  `verify-disk-assets.sh` passing is not one gate for all three, because that
  script's missing-utility branch is severity-split by image (only an image over
  8 MB carrying a `55 AA` MBR reaches `bad()`), so a PASS is compatible with the
  games disk still having neither utility, and following a single-condition
  comment would have deleted the one sentence of the three that was still true.
- **`docs/CONFIGURATION.md` loses "a mistyped Reset reboots the machine without
  asking"**, in both places it said it, and gains a section on the configuration
  report. `Config.h` carried the identical claim as the stated justification for
  `ctrlRToCpm` being the one keyboard default that goes the other way, and is
  corrected too. The default itself stays true: the confirmation limits the
  damage of a keystroke the user did not mean, while reserving Ctrl+R would take
  a working CP/M key away every time they did.
- **`CODE_SIGNING.md` and `STORE_SUBMISSION.md` carry the two-stage packaging
  recipe** - rehearse unsigned, then sign only on a version that has not
  shipped, checking `Version.h` against the published releases first, because
  nothing in the script does.

- **`docs/CONFIGURATION.md`'s "Wrong kind of value" bullet was half the opposite
  of what happens.** It still said "the next save of any kind writes the built-in
  defaults over the section that was skipped" and that "simply quitting the app
  is enough to lose the section". `ConfigManager::loadFromFile` keeps the skipped
  section's own text in `AppConfig::unreadSections` and `to_json` splices it back
  at the pointer it came from, so the very saves the bullet listed as the danger
  - the window placement at close, the welcome flag, `SYSCONF`, *Start* on a
  config with no disks - are the ones that write the user's text back. The bullet
  says so now and quotes `renderBlock`'s own sentence for it, since the user sees
  both and they had better agree word for word. The "fix it first" instruction
  stays, moved onto the half still true: while a section is carried, nothing the
  application puts there reaches the file either, so a disk mounted while
  `"disks"` is an object is gone at the next save.
  The two limits are what the bullet never had, and they are the reason
  `todo.txt` kept it open. **A carry goes back only to the file it came from** -
  `saveToFile` clears it when `AppConfig::unreadSectionsFrom` does not name the
  path being written - so a profile saved out of such a config gets the built-in
  defaults for that section and loads cleanly, which is exactly the different
  file a user would otherwise be surprised by. **And a document that is wrongly
  typed *and* unreadable is quarantined with its carry dropped**: `inspectDocument`
  marks those diagnostics not-carried from the same readable test the loader
  uses, and `renderBlock` prints "not kept - the file it is in could not be read
  as a whole" on the line with a different closing sentence under the block. The
  example given for that case is a `disks` entry with no `path` rather than a
  syntax error, and the distinction is measured rather than assumed: a file that
  will not parse never reaches `inspectDocument`, so it raises no section-level
  diagnostic at all to be marked. Five shapes were run through `inspectDocument`
  and `renderBlock` in a scratch program to check every sentence the file now
  quotes.
- **`MANUAL_CHECKS.md`'s "keys as an array" step gets the two lines it was
  missing** - the block is still there after pressing OK in *Settings*, and still
  there after a restart - and loses a claim that was never true: the keymap does
  not "come up empty", because `load()`'s fill loop puts every default binding
  into the map `from_json` left empty. The "banana" step was quoting a label the
  app does not print, "not recognised:"; `problemLabel()` renders `UnknownMember`
  as "unrecognised setting:". The profile step gains the behaviour that changed
  under it - a profile that will not read leaves the report about the file still
  in force standing.
- **`FEATURE_PARITY.md`'s rows 2 and 3 were corrected after all three ports were
  read side by side for the first time**, prompted by `ioscpm` turning out to
  have a scrollback that had never captured a line. Row 2 scored `ioscpm` a bare
  tick from 2026-07-25 over a feature that had never held any history, and row 3
  scored it a tick over a port with **no text selection at all**; both became
  true on 2026-09-02 at build 57 and not before, and both cells say so rather
  than reading as though they always had been.
  **The "Verified Android behaviour (2026-08-07)" block was substantially
  fiction.** Nine symbols it cited exist nowhere in `cpmdroid`, in the tree or
  anywhere in its history, and four claims resting on them asserted the opposite
  of what that code does - the view is not anchored as output arrives, the cursor
  is not hidden over history, and no history is drawn at all while the soft
  keyboard is up. Its clipboard cap described nothing: neither number appears in
  that repository. The block is rewritten from a citation-by-citation re-read,
  the real findings kept, and the inventions **named** so the next reader knows
  what happened - which is what `cites-withdrawn:` now enforces mechanically.
  **The correction then had to be corrected**, and that is the part worth
  keeping. Its first pass kept the old block's claim that `cpmdroid` captures at
  `scrollRegionUp`, which it does not - `scrollUp()` does the four `addLast`
  calls and is the only path into the history - and kept the old condition, "only
  when the scrolling region starts at row 0", which is not what the code says and
  contradicts its own status-line rationale. The guard is the *whole screen*,
  which is the same shape as this repository's `scrollRegionUp` and as `ioscpm`'s
  `scrollRegion` since build 57, so all three agree and `cpmdroid` always did.
  The third error is the same failure the whole pass was about: it listed
  `cpmdroid` keeping history across a cold boot as a **defect**, when `clear()`'s
  docstring says the opposite at length and names how the siblings differ. The
  port had considered exactly this and decided against it, in a comment, and the
  correction asserted a bug without reading it. So "history cleared on emulator
  start/reset" is a line where the three ports disagree *on purpose*. Reading a
  port from outside is harder than it looks even when you know that is the
  failure mode.
- **Three Android rows move, and the two capabilities with no row get one.**
  Rows 4, 6 and 13 were re-read against `cpmdroid`; all three had been that
  port's weakest cell in their row and none of them is now. Row 13 is the big
  one: this document called that parser "the thinnest of the four" and was right
  when it was written, and it now has VT52 with the same auto-detection this repo
  uses, DECSTBM with a region-aware line feed, DECSC/DECRC saving the rendition
  as well as the position, the seven editing finals, the query replies, the three
  private modes, deferred autowrap, and per-cell attributes carrying **this
  repository's three bits with the same values**, so the two ports' cells can be
  diffed directly. The tick carries a qualifier the other three columns in that
  row do not, and the row says it rather than letting a reader assume otherwise:
  that work is compiled and has **never been run**.
  A bounded import and an inbound share went into row 4 rather than a fourteenth
  row, because a new row would have renumbered every "item 13" and "(#4)" in a
  file that has dozens of them, against a catalog the status table, the priority
  list and seven rows all depend on. Writing the third transfer target - "let the
  OS hand the app a file" - turned up something in **this** repository: the first
  draft called it not-applicable for Windows, "no such OS affordance to wire up",
  and the evidence against that is in this tree. `packaging/msix/AppxManifest.xml`
  already declares `windows.fileTypeAssociation` for `.img`/`.dsk` and `.rom`, so
  a Store install offers this app as an Open-with target - while `main.cpp` takes
  `lpCmdLine` and immediately `UNREFERENCED_PARAMETER`s it, and no window accepts
  `WM_DROPFILES`. Double-clicking a disk image starts the emulator and loads
  nothing. That target is open on Windows, and worse than untouched.
- **Both sibling columns re-read across all thirteen rows**, thirty-five
  corrections, and the `sibling-readings` block advanced to match. The one worth
  naming is row 1, where the document said `ioscpm` has "no modifier concept at
  all" and that Ctrl+Left cannot be bound separately from Left. That closed on
  2026-08-27 - `SpecialKey` carries `ctrlUp`/`ctrlDown`/`ctrlLeft`/`ctrlRight`,
  twenty-six cases not twenty-two - and this document had read that very commit
  for row 13 alone, the erase family and `applySGR`, and nothing else. **A
  reading scoped to the row somebody mentioned is how a column rots while looking
  maintained.** Row 13 lost several claims the same way.
  Provenance, because this document is about not overstating readings. Rows 2-7,
  9 and 12 were corrected by an agent and independently checked by a second one
  against the code; row 1's checker found four errors in the proposal but
  returned no usable text, so that row was verified and rewritten by hand; row
  13's checker died on a session limit and its seven claims were verified by hand
  instead. All of it is a source reading. Nothing in either column was watched
  running except `ioscpm`'s scrollback and selection, and `cpmdroid` has no SDK
  on this machine at all.
- **The `romwbw_emu` column re-read, and a caveat that was false before it was
  written.** Thirteen corrections. The one that matters spanned the whole web
  column: xterm.js, its CSS and the fit addon were described as three jsdelivr
  tags with no vendored copy and no SRI, so an offline page had no terminal at
  all. That was vendored under `web/vendor` on 2026-08-26 with the template
  pointed at it, and `release.yml` stages the directory into the packages. **The
  date is the point.** That commit is an ancestor of the one the readings block
  recorded, so the caveat was already false on 2026-08-27 when the column was
  last read and *certified*. Row 1's error the week before was a commit read for
  one row and not the others, which is a scoping failure; this one was never
  checked at all, in the paragraph the column gives the most prominence to. A
  reading that certifies a whole column is worth what its least-checked paragraph
  is worth. Also corrected: row 9's web half hardcodes a font size in the
  template with no control; row 13's web cell overstated, since the page hands
  bytes to `term.write()` unchanged but the build does not; row 5's web half; and
  row 10's Dazzler absence re-confirmed. One claim where an agent and I disagreed
  went to the code and the agent was right: what reads as an integrity attribute
  in the template is a comment explaining why SRI is deliberately absent for
  same-origin files.
- **The citation rule has been run over all thirteen rows** - 25 marked regions,
  up from two - and marking them is what corrected the checker rather than the
  other way round. Multi-port blocks are marked per sub-bullet, so a claim about
  Android and a claim about iOS in the same item are checked against different
  trees. Four things the real document taught the check: the bare-component
  fallback was a false-pass machine, making `` `Keymap.h` `` pass by searching for
  "h" and `` `w8.com` `` pass on "com", so a component under four characters no
  longer counts - a false pass is worse here than a false failure, since the
  whole point is to catch a citation nobody checked. File-shaped tokens are
  looked up as paths rather than content, because grepping file *content* for
  "MANUAL_CHECKS.md" finds nothing however real the file is - which is how that
  file came to be reported as a fabrication. A path not in the tree falls through
  to a content search instead of failing, because `jni.h` is the NDK's header and
  a name can be a true citation while belonging to somebody else's tree. And the
  recorded-sha test and the origin test go through one resolver now, so they
  cannot disagree about what resolving means; they did.
  The extractor was also skipping the way this document names a function.
  `` `foo()` `` is the near-universal form here and only bare identifiers were
  tested, so 25 citations inside the marked regions - an eighth of them - were
  never checked at all. It was noticed the honest way: a sentence had just been
  written into the `ioscpm` scrollback block, the checker reported clean, and the
  reason it reported clean was that it had not looked. Every citation in the
  document now resolves in the port it describes, at the commit that port's
  column was read at: **zero failures, down from twenty-five, without a single
  one being suppressed.** Verified in both directions by injecting a fabricated
  symbol into a marked block - caught, exit 1.
- **The stale `[MAC]` item came off `todo.txt` by grepping the tree it
  described.** The SGR 90-97 gap it recorded had closed upstream, and `applySGR`
  is not in the file the item named any more. Every cross-port item in that file
  is a claim about somebody else's code and nothing re-reads them, which is the
  argument the citation rule above exists to answer.

### Verified
- **The tree builds against the v1.36 core.** Both configurations, against
  `romwbw_emu` `17cd380` (`v1.36-1`) and `cpmemu` `9fee3c2`. The link error the
  sync was supposed to produce did produce - `unresolved external symbol
  emu_host_path_caps`, from `hbios_dispatch.obj` - and defining the function is
  what cleared it.
- **All three headless suites pass**, 354 checks at that pass: 252 terminal, 66
  host-file backend, 36 HBIOS. (The terminal suite is 268 now - the ANSI/CGA
  colour entry above adds 16 - so the current total is 370.) Both new suites
  were checked against a deliberately broken build before being believed -
  reverting `emu_host_file_get_write_name()` to the old echo fails 9 of them,
  and resolving the redirection through the file instead of its parent fails
  9 more, including the exports that then fail for
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
- **The disk-image verifier was run both ways, 2026-08-27.** Against
  `romwbw_emu/disks/` it exits 0: four binaries match the source and both
  `w8.com`s carry the probe. Against **the images this project actually ships**
  it exits 1. Those images are not in this repository, so they were taken from
  the published `z80cpmw-1.0.22-beta.msix` — an MSIX is a zip, and it carries
  `disks/hd1k_combo.img` and `disks/hd1k_games.img` — and unpacked into a scratch
  directory. `hd1k_combo.img` holds a 1079-byte `r8.com` where the source now
  builds 1792, and a `w8.com` with no probe in it whose only usage string is
  `Usage: W8 <cpmname>`, with no `[hostpath]`: the `W8` from before `romwbw_emu`
  `98eb6a1`. `hd1k_games.img` carries **neither utility**, so no host file
  transfer works from that disk at all — which nothing had recorded. The three
  diskdef and geometry guards were exercised too, against a truncated combo
  image, a 4 MB file and an 8 MB blank: each is refused with its own reason
  rather than read as an empty directory. `todo.txt` keeps the rebuild open.
- **The drift script's four outcomes were each tested against doctored input**
  in a scratch directory — seven throwaway clones with a real `origin`, covering
  current, drifted, an unrecorded commit, a recorded commit that is not an
  object, a checkout behind its origin, a clone with no `origin/HEAD` (the
  fallback path), and one with no origin at all. The committed version of the
  script was run against the same input for comparison and reports the
  behind-origin clone as **"current"**, which is the bug. `--fetch` was
  exercised against those scratch clones only. Against the real siblings the
  script now exits 0.
- **The tree builds with MSVC, and the three never-compiled changes are
  settled.** `emu_host_file_get_read_name()` in `emu_io_windows.cpp` links,
  `ansiToCGAColor()` in `TerminalView.cpp` compiles, and both packaging scripts
  parse - 1362 tokens for `build-msix.ps1` and 729 for `build-nsis.ps1` through
  `[Parser]::ParseFile`, which is the check that could not be run anywhere else.
  The app was booted five times to the ROM's `Boot [H=Help]:` prompt.
- **1020 checks in six suites, all passing** from `tests\run_tests.bat`:
  terminal conformance 516, help renderer and assets 244, rendering conformance
  50, host file transfer 66, HBIOS host file extension 36, configuration
  diagnostics 108. It was three suites and 370 checks that morning.
- **The manual check comes off `MANUAL_CHECKS.md`.** The item that "wants a
  person looking at a real screen and agreeing that `ESC[31m` is red" is
  machine-checked now: `tests/test_render.cpp` renders a real window, asks the
  DWM for it with `PrintWindow` and samples the pixels. It found the SGR 90-97
  hole on its first run, which is the argument for it.
- **Mutation-tested, with the counts.** Seventeen mutations of the help cache,
  seventeen killed. Ten of the help renderer, ten killed - removing the fence
  branch fails 7, letting a fence's content through the markdown branches 4,
  un-doing the bullet fix 3, the table fix 2, the header fix 2, reverting
  `toWide` 3, and the four rules of `isSafeAssetName` 21, 4, 3 and 1. Eight of
  the paint path, 23 checks killed, plus 52 consecutive clean runs and 10 more
  at 200% DPI, which is what caught a check that assumed a 1200px update region
  on a window only 874 wide. And against a patched `Config.cpp`, removing the
  save suppression fails 4, reverting the type check 10, and reverting the
  unknown-name report and the non-clobbering backup 16. One mutation of the
  eight is deliberately *not* caught, and the comment says so rather than
  implying the choice is pinned: taking the character metrics from the bold face
  instead of the normal one moves nothing today, because `tmAveCharWidth` is
  identical for all four Consolas faces at every integer height from 8 to 96.
  `tmMaxCharWidth` is not - 15 regular against 16 bold at height 16 - so the
  agreement is a property of this font rather than a rule, which is why the
  index is pinned at 0 anyway.
- **The unsigned beta rehearsal was run, and it is what `todo.txt` should have
  asked for.** `build-msix.ps1 -Beta -SkipBuild -SkipSign` exits 0, writes
  `dist\z80cpmw-1.0.22-beta-unsigned.msix` and its `.pdb` (hash-equal to
  `bin\Release\z80cpmw.pdb`), produces a package that reads `NotSigned`, and
  leaves the published `dist\z80cpmw-1.0.22-beta.msix` on hash `1549C223`
  before and after. Both `-unsigned` files were deleted afterwards; they are
  build output. **No signed `-Beta` run was made deliberately, and none may be
  made while `Version.h` says 1.0.22** - the one that did happen today happened
  because `-WhatIf` was swallowed, and is recorded under **Changed**. **1.0.22's
  symbols are not recovered**: `dist\` holds no `z80cpmw-1.0.22-beta.pdb` and
  cannot come to hold one.
- **What is not covered, said plainly.** `MainWindow.cpp` and
  `SettingsDialogWx.cpp` are in no suite, and the paint path's only coverage is
  the rendering suite, so the Reset confirmation, the notice lifetime and its
  retractions, the profile-failure report, the Settings pages and the Keyboard
  page were checked by building the app into a private directory and driving it
  by hand with `WM_COMMAND` and `PrintWindow`: `ID_EMU_SETTINGS` opens the
  dialog, each tab clicked at its centre reports `TCM_GETCURSEL` 0, 1 and 2,
  captures show every page with the status line and OK/Cancel on all of them,
  every control member is constructed exactly once, and the real `z80cpmw.json`
  was restored byte-identical afterwards. That is one person watching one
  machine, not a check that will notice a regression next month. Inside the help
  window, the offline arm needs WinHTTP to fail machine-wide, and the message
  ordering and the three other widening sites need a message loop, so those are
  argued against the source rather than measured - the truncation *rule* is
  tested, against a throwaway localhost server, while the wiring inside
  `downloadToString` was checked by probe. The Dazzler group in Settings and
  `settings.debugMode` were both re-confirmed broken and deliberately not
  touched; `todo.txt` has them. *(Corrected 2026-09-03: both were fixed later
  the same day, in "Three controls in Settings that were reading and writing
  nothing" under **Fixed** above, and this sentence went on pointing at a
  `todo.txt` that no longer had them for six days. It survived a changelog pass
  written after the fix landed — which is the same failure the `FEATURE_PARITY.md`
  entries below are about, in this file.)*
- **A note for the next person driving this app from a script**, since that is
  how half of the above was checked and it cost two crashes and two dumps: a
  common-control message that carries a **pointer** - `TCM_GETITEMRECT`,
  `LVM_GETITEMTEXTW`, `LVM_SETITEMSTATE` - is not marshalled across a process
  boundary, so sending one from a driver process makes the app dereference the
  driver's address and die inside `comctl32`. Use `VirtualAllocEx` /
  `WriteProcessMemory` in the target, or stay with pointer-free messages. Call
  `SetProcessDPIAware()` in the driver first, or DPI virtualisation halves every
  rectangle and the 900x819 dialog reads as 450x410, as though `SetSize` were
  being ignored. And wx's notebook tab class is `_wx_SysTabCtl32`, not
  `SysTabControl32`.

- **1323 checks in six suites**, which is four suites out of date from the 1020
  recorded above: terminal conformance 516, help renderer and assets 353,
  rendering conformance 50, host file transfer 66, HBIOS host file extension 36,
  configuration diagnostics 302. All six were built and run with their own `/Fo`
  and `/Fe` under a scratch directory rather than through `tests\run_tests.bat`,
  which shares `obj\`.
- **This port has no catalog-invalidation wipe, which nobody had checked.**
  `romwbw_emu` `docs/RELEASE_ORDER_2026-08-25.md` adds a second data-loss path to
  its step 5 - `ioscpm` compares `disks.xml`'s `version` attribute against a
  stored default on every successful fetch and, on any difference, deletes every
  `.img` in its disks folder, including images the user imported or created,
  which are in no catalog and cannot be re-downloaded. It fired in the field once
  already. That document asks for `cpmdroid` and `z80cpmw` to each be checked for
  the same behaviour before their own step 5, and this is that check for this
  port: `DiskCatalog.cpp` never reads the `version` attribute at all - it is
  absent from `parseCatalogXML` and from the whole file - and its only three
  deletes are two partial-download cleanups and `deleteDownloadedDisk()`, which
  is one file at the user's request. **The hazard does not exist here**, so the
  catalog pin can move on this port without an answer to it. That is a fact about
  this backend rather than a decision, and it is recorded so the next person does
  not have to re-derive it.
- **Repinning the catalog to `v1.4.12` changes exactly one image.** The refreshed
  disk images were published on `avwohl/ioscpm` as `v1.4.12` on 2026-09-01, 29
  assets, as a prerelease - so `releases/latest` is untouched and no installed
  client sees anything. Both catalogs were fetched and diffed here: `v1.4.5`'s
  `disks.xml` and `v1.4.12`'s are **7042 bytes each and differ on one line**, the
  `<sha256>` of `hd1k_combo.img`, from `be19984e…` to `89b8ae1a…`. Every other
  filename, size and hash is identical, and so is the `version` attribute - which
  matters twice over: nothing else about the disk set changes, so no
  HBIOS/CBIOS pairing moves, and on a port that *did* have the invalidation wipe
  this would not have fired it either. The staged `bin\Release\disks\hd1k_combo.img`
  hashes `be19984e…`, so the image bundled in the package today is byte-for-byte
  the unfixed one the catalog served, which is what `todo.txt` said and is now
  measured rather than inferred. The staged `hd1k_games.img` hashes
  `7f33738c…`, which is exactly what *both* catalogs record for it, so that image
  is already current and the refresh touches one file - it still carries neither
  `R8` nor `W8`, and that caveat stays true wherever it is written down.
  `releases/latest` was checked independently rather than taken from the release
  order's word: it redirects to `v1.4.11`, so the prerelease really is invisible
  to installed clients.
  `packaging/scripts/verify-disk-assets.sh` could **not** be run: `cpmls` is not
  on `PATH` on this machine and the script exits 2 rather than guessing. The
  refresh is not finished until it passes on the staged images somewhere that has
  cpmtools.

## [1.0.25] - 2026-09-04

**A user is now told when the disk image on their machine is no longer the one
the catalog serves.** 1.0.24 repins `RELEASE_TAG` to `v1.4.12`, and that decides
what a *new* download gets and nothing else: a machine that already had
`hd1k_combo.img` kept the old `R8` and nothing anywhere said so. This is the
half of that hole which can be closed in this repository.

Release x64, MSBuild 18, **0 warnings and 0 errors**, and **seven** headless
suites green - **1467 checks, 0 failures** (terminal conformance 516, help
renderer and assets 355, configuration diagnostics 302, **disk provenance 142**,
host file transfer 66, rendering conformance 50, HBIOS host file extension 36).
`dist\z80cpmw.msix` is the unsigned Store package, 7,090,707 bytes, and it was
**verified at the artifact rather than at the tree**: `tools/check-disk-pins.sh`
reports `z80cpmw.msix carries v1.4.12, agrees with the tree`, its
`AppxManifest.xml` reads `Version="1.0.25.0"` with the Store identity
`Publisher="CN=724C9014-…"`, and it holds **0 entries matching `*.img`** — the
"nothing is bundled" rule checked in the package instead of asserted from the
script. No `-beta` has been cut; if one is, it must come off this same
`bin\Release` with `-SkipBuild`.

1.0.24's package was **not** overwritten by that build. `build-msix.ps1` writes
the fixed name `dist\z80cpmw.msix` and deletes whatever is already there, so it
was copied to `dist\z80cpmw-1.0.24-store.msix` first — 7,074,904 bytes, sha256
`6320364e…`. It is still submittable.

- **`DiskLedger` decides staleness from provenance, not from bytes**, and that
  distinction is the whole design rather than an implementation detail. It is
  **ported from `ioscpm`'s `iOSCPM/Views/DiskLedger.swift`**, which solved this
  first; `todo.txt` said the first port to solve it was worth copying rather
  than re-deriving, and this is that copy. What is dropped is the iOS-only half
  - `NetworkCondition` and the expensive/constrained deferrals, which are about
  somebody's cellular plan. What is kept is every decision that can lose work.
  **The obvious fix destroys user data.** "Hash the file, re-download when it
  differs from the catalog" is wrong in the direction that loses work, because a
  downloaded disk is a *writable CP/M volume*: the emulator writes the guest's
  changes straight into `data\hd1k_combo.img`, so the first time somebody saves
  a file inside a catalog disk its bytes stop matching the catalog for ever -
  and it is not stale, it is theirs. So the ledger records **the catalog
  `<sha256>` that a verified download actually matched**, which local writes
  cannot change: `superseded` is *recorded provenance != the catalog's current
  hash*, and `pristine` - do the bytes still hash to what we recorded - decides
  only whether replacing them is lossy. Only a superseded **and** pristine image
  is ever a candidate for replacement without asking.
  **Migration is the honest limit.** Every install in service has no ledger,
  because nothing has ever written one, so an image that does not hash to the
  catalog is either the superseded one or the user's own work and *there is no
  evidence that separates them*. Those get a verdict of "Differs from catalog"
  and never the automatic path. One case does resolve on its own: an image that
  already hashes to the catalog is current whoever fetched it, so its provenance
  is adopted on sight - which is what stops a migrating install re-hashing
  nineteen of the twenty entries every time the catalog is fetched.
- **`<sha256>` is parsed at last.** `parseCatalogXML` read `filename`, `name`,
  `description`, `license` and `size` and skipped the hash the catalog has
  always published, so nothing could compare anything: `isDiskDownloaded` is a
  size floor (`size >= expectedSize`), and **both `hd1k_combo.img` images are
  51,380,224 bytes** and differ in 5,121 of them, so no size check could ever
  have seen the repin. An `<sha256></sha256>` and a missing element both parse
  to the empty string, and telling either from a usable hash is
  `DiskLedger::normalizedHash`'s job in one place rather than each caller's.
- **A download is now verified against the catalog's hash before it counts.**
  `downloadToFile` compared the byte count against `Content-Length`, which
  passes a transfer that is the right length and the wrong bytes. A mismatch now
  deletes the file and fails the download - and it has to, because provenance is
  recorded only for a transfer this code verified: a recorded hash the bytes
  never had would read as `Current` for ever. An entry whose catalog carries no
  usable `<sha256>` still installs, unverified and without provenance, so an
  older catalog than this build expects keeps working.
- **Settings → Disk Images says which of the five things is true.**
  `DiskLedger::describe` is the only place the wording lives, so a verdict
  cannot be reworded in one dialog and not another: "Available", "Downloaded",
  "Update available", "Update available (overwrites your changes)", and
  "Differs from catalog" for the ambiguous migration case - which says what is
  known rather than accusing the user of being out of date.
- **`DiskHash` exists so the measurements can be checked.** `sha256File` and
  `statFile` were written as private statics on `DiskCatalog`, where no suite
  could reach them, and **neither failure is visible by reading**: a wrong hash
  marks every image in the library as differing from the catalog, and a write
  time read the wrong way re-hashes 211MB on every launch for ever. Split into
  `DiskHash.cpp` - Win32 and the CRT, no WinHTTP, no wx, no `DiskCatalog` - the
  suite links them, and it checks the hash against **FIPS 180-4's own vectors**
  for `""` and `"abc"` rather than against the code agreeing with itself. The
  64KB read loop is checked at 65535, 65536, 65537 and 200000 bytes against
  BCrypt's *one-shot* API, which chunks nothing; a loop that stopped after one
  block, or mistook a read error for EOF, would return a confident hash of a
  prefix. The write time is stored as raw `FILETIME` ticks, as an integer all
  the way to the JSON, for the reason the ported comment gives: a time that
  round-trips a hair off invalidates every measurement on every launch.
- **The threading contract is unchanged, and that took the work.**
  `updateFreshness()` reads up to 211MB, so it runs on the `fetchCatalog`
  worker and never from `updateDownloadedStatus()` - which is *also* called by
  `setDownloadDirectory()` on the UI thread, and hashing the library inside the
  Settings dialog's OK handler would freeze it. `m_ledgerMutex` is a third
  mutex rather than a share of either existing one, and nothing holds two at
  once: every user copies the ledger out, works on the copy and assigns it back.
  The write-back **merges** rather than assigns, because a download that
  completed while the hashing ran has already written a provenance record that
  the copy taken at the top does not have. The ledger file is written through a
  temporary and `MoveFileEx`'d into place; a truncated one deserialises to an
  *empty* ledger rather than a half one, since a stale measurement against the
  wrong file reads as a verdict while a missing record only costs a re-measure.
- **`DiskCatalog.cpp` still never reads `disks.xml`'s `version` attribute.** The
  check recorded under 1.0.23 - that this port has none of the
  catalog-invalidation wipe that deleted user-imported images on `ioscpm` -
  stays true: this release adds a `<sha256>` reader and no attribute reader.
- **`disks/cpm_wbw.img` and `disks/zsys_wbw.img` are deleted**, 13,824 and
  10,240 bytes, and the `disks/` folder with them. They were what the deleted
  `MainWindow::loadDefaultDisks()` wanted, no packaging script has staged them
  since dbd53b1, and nothing in the tree names them: `Grep` over the sources,
  the tests, `tools/`, `build*.bat`, the `.vcxproj`, the `.nsi` and the
  packaging scripts returns only prose about their deletion. `.gitignore`'s
  commented-out `# disks/*.img` is now a real rule, so the standing "disk images
  come from the ioscpm release area, always" is enforced rather than remembered.
  The two comments that described `loadDefaultDisks()` in the present tense -
  in `z80cpmw.nsi` and `packaging/STORE_SUBMISSION.md` - are corrected.
- **`build-msix.ps1` keeps the Store package's `.pdb`.** Step 6 was
  `if ($Beta)`, so the symbol copy — and the `Write-Error` that fails the run
  when there are none — covered the sideload beta and **not** the Store package,
  which is the binary most users run. `CLAUDE.md` has said "both packaging
  scripts keep the `.pdb` beside their output and fail if it is missing" since
  the 1.0.22 post-mortem, so the rule was true of the documentation and false of
  the code, on the vehicle nobody re-read. The reasoning that left it that way —
  "the beta is the binary testers actually run" — is a reason to cover the beta,
  never a reason to leave the Store arm uncovered; Microsoft re-signing a package
  does not change what a crash dump from it needs. It now runs on every arm.
  The name is versioned, `dist\z80cpmw-<ver>-store.pdb`, because the Store
  package's own name is not: an unversioned `z80cpmw.pdb` would be overwritten by
  the next Store build, which is exactly how a shipped version's symbols are lost.
  **The cost of the gap is already sunk for 1.0.24.** No `.pdb` for it exists
  anywhere — `dist\` holds only `z80cpmw-1.0.21-beta.pdb` and
  `z80cpmw-1.0.23-beta.pdb`, and `bin\Release\z80cpmw.pdb` has since been rebuilt
  at 1.0.25 — so if 1.0.24 is submitted, its crash dumps will be unreadable the
  way 1.0.22's are. 1.0.23 escapes only by accident: its beta was cut from the
  same build, so `z80cpmw-1.0.23-beta.pdb` symbolicates the Store binary too.
- **`Version.h` moves 1.0.24 → 1.0.25**, which is the whole of what a version
  bump touches: the scripts parse those four `#define`s and the committed
  `AppxManifest.xml` carries a `0.0.0.0` placeholder.

**What this release does not do**, said here rather than discovered later.
Nothing acts on `DiskRefreshPlan::RefreshNow` yet - the ledger names the images
that could be replaced without losing anything, and no caller replaces them,
because `isMounted` is `MainWindow`'s fact and replacing a file the running
machine holds in a slot undoes itself on the next flush. And a user who never
opens Settings is still not told: `downloadAndStartWithDefaults()` loads what
`diskFileLooksComplete()` finds and never fetches the catalog, so a boot-time
notice would mean a network fetch at every launch. Both are in `todo.txt`.
`MainWindow.cpp` and `SettingsDialogWx.cpp` are in no suite and cannot be, so
the status column compiles and has **not** been seen on a screen - it is in
`MANUAL_CHECKS.md`.

## [1.0.24] - 2026-09-03

**The release that actually delivers the `R8` fix to Windows users**, which
1.0.23 did not. Everything below was in the tree when 1.0.23 was packaged and
missed the build; the repin is the whole point of cutting this.

Release x64, MSBuild 18, **0 warnings and 0 errors**, `bin\Release\z80cpmw.exe`
reporting `1.0.24.0`, and all six headless suites green - **1325 checks, 0
failures** (terminal conformance 516, help renderer and assets 355,
configuration diagnostics 302, host file transfer 66, rendering conformance 50,
HBIOS host file extension 36). `dist\z80cpmw.msix` is the unsigned Store
package.

**Verified at the artifact, not the tree**, which is the check 1.0.23 did not
have: `tools/check-disk-pins.sh` reports `z80cpmw.msix carries v1.4.12, agrees
with the tree`. The tree being right is not the same as the package being right,
and last time the difference cost a release.

**How this came to be a separate release.** These changes were in the tree when
1.0.23 was packaged, and the packages were built before them - so 1.0.23 shipped
carrying `RELEASE_TAG = v1.4.5` and the unlocked `m_downloadDir`. That was said
in the changelog, said in the commit, and said out loud, and it shipped anyway.
A note that has to be read at the right moment is not a gate, which is why
`tools/check-disk-pins.sh` now looks at the built artifact and why this release
was verified against the package rather than the source.

- **`todo.txt` is gone, and this is what emptying it consisted of.** It had
  grown for six rounds because each pass added cross-repository notes faster
  than it closed anything, and most of what accumulated was neither a bug nor a
  feature for this client. The standing rules moved to a new `CLAUDE.md` (where
  they can be followed rather than re-read as tasks); items about `cpmemu`,
  `romwbw_emu`, `cpmdroid` and `ioscpm` were deleted, because each of those has
  its own list and a note here could never be actioned from here; and the
  remainder were fixed rather than reworded.
- **The disk catalog is repinned to `ioscpm` `v1.4.12`.** `RELEASE_TAG` in
  `DiskCatalog.cpp` is this port's own choice of which ioscpm release its users
  download, and it is the *whole* of what this repository contributes to the
  question. It said `v1.4.5` while `v1.4.12` existed, so the defect was never
  "`R8` is broken here" - `R8` and `W8` are CP/M programs on a published disk
  image, fixed in `romwbw_emu/src/r8.asm` and built by ioscpm, and nothing in a
  z80cpmw build contains either. The defect was that this port served an older
  release than the one available. Each port pins independently, so each can rot
  independently; that is the cost of the pin, and the ROM pairing below is what
  it buys.
  What the newer release carries, verified in the **published bytes** rather
  than in the lineage: the `v1.4.12` `hd1k_combo.img` (sha256 `89b8ae1a…`,
  downloaded and hashed here) contains `Usage: W8 <cpmname> [hostpath]` and the
  `06 E9 CF` bytes of the `HBF_HOST_CAPS` probe, where the `v1.4.5` image
  contains neither. Upstream's `fcb_char` now maps `?` and `*` out of the FCB
  before `F_DELETE` sees it - the old `R8` erased every matching CP/M file when
  importing a host file whose name held either - and `w8.asm` defers EOF runs
  instead of stopping at the first `1Ah`.
  **The pin's own justification was checked rather than assumed**, because
  moving it is the exact thing its comment warns about: a new release swapping
  images out from under an installed client and re-introducing an HBIOS/CBIOS
  mismatch with the ROM this port embeds. Byte-diffing the two images: **5,121
  bytes differ out of 51,380,224 (0.01%), in three regions** - the CP/M
  directory entries for `R8 COM` and `W8 COM`, whose extents and record counts
  moved because the files grew; `R8.COM`'s data; and `W8.COM`'s data, which was
  all zeros in `v1.4.5`, consistent with the `org 0100h` padding that release
  removed. **The first differing byte anywhere is 1.02 MB in**, so the boot
  tracks, the HBIOS/CBIOS system area and the CP/M system image are
  byte-identical. The pairing the pin protects is untouched.
  The two catalogs are also 7042 bytes each and differ on one line, so no other
  image in the set moves, and the `version` attribute is unchanged - which is
  what would trigger a disk wipe on a port that has one (this one does not).
- **The three documents gated on that repin are updated**, each against its own
  condition rather than all at once. `README.md`, `docs/FILE_TRANSFER.md` and
  the bundled Help topic lose the "W8 does not take a host path yet" paragraph
  and the "Two cautions until then" block; `docs/FILE_TRANSFER.md`'s capability
  table and its "I can't find my exported file" checklist lose the step-0/1
  caveats; and `MANUAL_CHECKS.md`'s struck-out `W8` check is re-instated with
  the syntax to type. **The games-disk sentence stays** in all three - it was
  never gated on the same thing, and `v1.4.12` re-uploaded `hd1k_games.img`
  byte-identical (`7f33738c…`), so it still carries neither utility.
  The help suite **caught this**: two assertions existed to fail if a block was
  deleted before its condition was met. They are now inverted - they pin the
  blocks *out*, so re-introducing a caveat about a `W8` that no longer ships is
  a failure too - and the games-disk assertion is untouched.
- **`DiskCatalog::m_downloadDir` is locked.** It was the one member with neither
  a lock nor a written argument for not needing one: written by
  `setDownloadDirectory()` and read unsynchronised by both workers and by
  `getDownloadDirectory()`. It was safe only because of *when* it happened to be
  written - a property of the call order, not of the class, and the class had
  grown a documented threading contract when the two use-after-frees were fixed.
  It gets its own `m_downloadDirMutex` rather than sharing `m_catalogMutex`,
  because `updateDownloadedStatus()` reaches `getDiskPath()` and would
  self-deadlock on a non-recursive mutex; `setDownloadDirectory()` releases the
  lock before calling it for the same reason, and uses the caller's own string
  rather than re-reading the member. The download worker takes **one** copy for
  the whole transfer, so the directory it creates and the path it writes to
  cannot become two different strings if the folder moves mid-download. The
  constructor's assignment stays unlocked and says why: no other thread can hold
  a reference before it returns.
- **`MainWindow::loadDefaultDisks()` is deleted.** It had no caller, and it
  looked in the install directory's `disks\` for `cpm_wbw.img`/`zsys_wbw.img` -
  filenames that were never staged into either package. Establishing that is
  what proved the bundled images were dead weight in the first place.
  `onFileSaveAllDisks()` loses an `appDir` it computed and never used.
- **`build-nsis.ps1` keeps the `.pdb`.** It kept none, so an NSIS-only release
  had its symbols nowhere recoverable - the same defect that lost 1.0.22's, now
  closed on the vehicle that still had it. It writes
  `dist\z80cpmw-<ver>-setup.pdb` and **errors** if `bin\<Configuration>\z80cpmw.pdb`
  is absent, after moving the installer so a build is not thrown away.
- **`v1.0.22-beta` is flagged prerelease**, matching 1.0.16, 1.0.17 and 1.0.21.
  It was the only beta marked Latest.

## [1.0.23] - 2026-09-03

The twenty-three source commits recorded under **[Unreleased]** above, cut as a
release. Everything in that section ships here; this entry records only what
changed at packaging time, and the version bump those commits were waiting on -
`Version.h` moves 1.0.22 → 1.0.23, which is what makes a signed `-Beta` run
legal again (1.0.22 is published on both channels, so a `-Beta` run under that
number would have re-minted a published artifact).

Built and packaged on a Windows machine: MSBuild 18, Release x64, **0 warnings
and 0 errors**, `bin\Release\z80cpmw.exe` reporting `1.0.23.0`, and all six
headless suites green from `tests\run_tests.bat` — **1323 checks, 0 failures**
(terminal conformance 516, help renderer and assets 353, configuration
diagnostics 302, host file transfer 66, rendering conformance 50, HBIOS host
file extension 36). `dist\z80cpmw.msix` is the unsigned Store package;
Microsoft re-signs it at submission.

### Fixed
- **A beta ships with its symbols, and this is the run that proved it.**
  1.0.22's `.pdb` was never kept — `dist\` holds none, a rebuild is a different
  binary with a different debug GUID, and a crash dump from the shipped 1.0.22
  is therefore unreadable and always will be. `build-msix.ps1` grew a step 6
  that copies `bin\<Configuration>\z80cpmw.pdb` out beside the `.msix` and
  errors if it is absent, so a beta cannot ship symbol-less in silence again.
  Until now that step had only ever run under `-SkipSign`, because 1.0.22 was
  published on both channels and a signed `-Beta` run would have re-minted a
  published artifact under its own name — `todo.txt` said in as many words that
  the first signed run on an unshipped version is what proves the rule end to
  end, and that 1.0.23 is where it happens. It happened: the run printed
  `Symbols kept: dist\z80cpmw-1.0.23-beta.pdb`, and that file is byte-identical
  to `bin\Release\z80cpmw.pdb` (`dfc1b9789fc71f03…`), so the symbols beside the
  package really are the ones built with the binary inside it. The item is
  closed.
  The signature was verified rather than assumed: `signtool verify` reports 0
  warnings and 0 errors, the Trusted Signing cert chains to a Microsoft public
  root (so testers need no dev-cert import), and the signature is timestamped,
  which is what keeps it valid after the leaf certificate expires — this one
  expires 2026-09-04, the day after the signing.

### Changed
- **No disk images ship in the package any more, and nothing was ever reading
  them.** Every port gets its disk images from the **ioscpm release area**,
  through the catalog pinned in `DiskCatalog.cpp`'s `RELEASE_TAG`. Both vehicles
  contradicted that: `build-msix.ps1` copied `bin\Release\disks\*` into the
  staging directory and `z80cpmw.nsi` installed `hd1k_combo.img` and
  `hd1k_games.img` into `$INSTDIR\disks`, and the published
  `z80cpmw-1.0.22-beta.msix` really does carry `disks/hd1k_combo.img` — verified
  by opening the package, and its bytes hash `be19984e…`, the unfixed image.
  **It was dead payload.** The only function that reads the install directory's
  `disks\` folder is `MainWindow::loadDefaultDisks()`, which looks for
  `cpm_wbw.img` and `zsys_wbw.img` — not the hd1k pair that was actually staged
  — and `grep -rn loadDefaultDisks z80cpmw/` returns exactly two hits, its
  definition and its declaration. It has no caller. There is no `CopyFile`
  anywhere that would stage a bundled image into the data folder either, and the
  other three `getAppDirectory()` call sites are all ROM paths. The path a real
  user takes is `downloadAndStartWithDefaults()`, which looks in the *user data*
  folder and downloads what is missing. So the images could only ever have been
  read by a function that does not run, looking for filenames that were not
  there.
  Removing them is a **size change with no behaviour change**: 12.7 MB → 7.07 MB
  packaged, 57 MB of payload gone. The NSIS uninstaller still deletes both files
  so installs that had them are cleaned up. What a user's `R8` and `W8` actually
  do is decided entirely by `RELEASE_TAG`, which is unchanged at `v1.4.5` here
  and is its own item in `todo.txt`.
  This also retires a standing claim. `todo.txt` and `STORE_SUBMISSION.md` both
  said the stale bundled `R8` meant "that destructive bug is live in what ships
  today". The bug is real and reaches users, but through the *catalog*, not
  through the package — the bundled copy was never opened. Both documents are
  corrected rather than deleted, because the overstatement is the kind this
  project has been correcting all week: it was inferred from what the packaging
  scripts copy, without checking whether anything reads the result.
- **`STORE_SUBMISSION.md` step 3 is no longer a gate.** It told the submitter to
  run `verify-disk-assets.sh` over `bin/Release/disks` and required exit 0; there
  are now no images in the package for it to check. The step records what
  replaced it and why, and keeps the pointer to the script, which is still the
  right tool for checking a set of images *before they are published* — that is
  `ioscpm`'s job, not this package's.

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

**Correction, 2026-08-26.** The second line above is wrong in both halves. It is
left standing rather than quietly edited away, because it is what this release
was announced as and because it is where the same claim in `README.md` and the
in-app help came from.

`89a4f28` changed the *host-side backend*: `isAbsolutePath` / `resolveHostPath`
in `emu_io_windows.cpp` stopped prepending the data folder to a path that was
already rooted. That part is real, and it is what made
`R8 C:\Users\me\Desktop\getkey2.com` work. It says nothing about `W8`, because
no `W8` ever handed it a path: `w8.asm` read the default FCB and nothing else,
and its usage line was `Usage: W8 <cpmname>`. `W8` did not take a host path
until `romwbw_emu` `98eb6a1` on **2026-08-24**, two and a half months after this
release - and the disk images this app bundles still carry the older utility, so
it is not true today either. The line should have read "R8 host transfers now
honor absolute paths".

The second half is early rather than invented. This release added
`emu_io_get_data_folder_display()` and showed the resolved folder in **Settings**
alone. **About** still showed the un-redirected `%LocalAppData%\z80cpmw`, and the
boot banner printed the literal template
`%LOCALAPPDATA%\Packages\<package>\LocalCache\Local\z80cpmw\data` rather than
anything resolved. `1f44e20` put the real path in both, and shipped in
**[1.0.15]** - whose own entry records only the About half of that.

`README.md` and `HelpWindow.cpp` still carry the `W8`-takes-a-path claim.
`todo.txt` lists where; both come right when the disk images are refreshed, and
neither is edited here.

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
