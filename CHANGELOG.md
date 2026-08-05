# Changelog

All notable changes to **z80cpmw** are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions use a simple `MAJOR.MINOR.PATCH` scheme: `-beta` tags are signed
GitHub / sideload prereleases, and unsuffixed versions are Microsoft Store
releases. The Store is on **1.0.14** (**1.0.18** submitted, pending review); the
**1.0.18-beta** signed MSIX carries the fixes and features listed below.

## [Unreleased]

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

[Unreleased]: https://github.com/avwohl/z80cpmw/compare/0146d94...HEAD
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
