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

[Unreleased]: https://github.com/avwohl/z80cpmw/compare/f91f0fd...HEAD
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
