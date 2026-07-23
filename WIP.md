# Work In Progress — z80cpmw

Working notes / handoff for the current round of changes. Not user-facing docs.

> **Historical (as of 2026-07-23).** This file captures the 1.0.15 round; the app
> is now at **1.0.17** (see `CHANGELOG.md`). The "open question" below about W8
> paths under MSIX is **resolved** — the behaviour is confirmed in source and
> documented for users in [`docs/FILE_TRANSFER.md`](docs/FILE_TRANSFER.md). Kept
> for context; not a to-do list anymore.

## Status

Version bumped to **1.0.15** (`Version.h`, `AppxManifest.xml`, `z80cpmw.nsi`).
Superseded — current version is 1.0.17.

Committed to `master` and **pushed** through `b4c36d2`:

| Commit  | What |
| ------- | ---- |
| 1f44e20 | Configurable keymap (termcap-style), mouse copy/paste, fix About data-folder path |
| bbab303 | Document config in README + docs/CONFIGURATION.md + online help; bump 1.0.15 |
| e463fb5 | Move startup instructions to scrollable Help (terminal has no scrollback); DPI-size Help window; first-run auto-open |
| db03984 | Remember main window position/size across runs (with monitor-change / off-screen reset) |
| b4c36d2 | Auto-size window to the 80x25 grid on font change (and as the default size) |

Installer built (untracked, in `dist/`): **`dist/z80cpmw-1.0.15-setup.exe`** (ProductVersion 1.0.15, bundles `bin\Release\z80cpmw.exe` 1.0.15.0). MSIX **not** rebuilt yet (`packaging/scripts/build-msix.ps1` available; manifest already at 1.0.15.0).

Local debug build for testing: `bin\Debug\z80cpmw.exe`.

## RESOLVED — where do W8 files land on the Store/MSIX build?

**Answer (confirmed in source; user doc: `docs/FILE_TRANSFER.md`):** full paths are
written verbatim even under full-trust MSIX; bare names go to `%LOCALAPPDATA%\z80cpmw\data`,
which the Store build redirects to
`...\Packages\AaronWohl.Z80CPM_<hash>\LocalCache\Local\z80cpmw\data`. The app
resolves and displays that real path (`emu_io_get_data_folder_display` →
`GetFinalPathNameByHandle`) in About, Settings, and the boot banner. The original
reasoning below held up; recorded here as the verification basis.

To be verified by **manual testing of the installed MSIX (Store) build** (the local build is unpackaged, so it can't reproduce the redirection).

### What the code does (verified in source)
- `z80cpmw/emu_io_windows.cpp`:
  - `resolveHostPath(name)` — if `isAbsolutePath` (drive-letter / UNC / rooted), uses the path **verbatim**; otherwise puts it in the data folder (`%LOCALAPPDATA%\z80cpmw\data`).
  - W8 writes via `emu_host_file_close_write()` → `fopen(fullPath, "wb")`. R8 reads via `fopen(..., "rb")`.
  - `emu_io_get_data_folder_display()` resolves the **real** redirected path (GetFinalPathNameByHandle) — used by About + Settings + the boot banner.

### Expected behavior (reasoning, NOT yet hardware-verified)
The app is a **full-trust packaged app**, not a sandboxed UWP app:
- `AppxManifest.xml`: `EntryPoint="Windows.FullTrustApplication"` + `<rescap:Capability Name="runFullTrust" />`.
- So it runs with the user's normal token/permissions → same file access as an unpackaged .exe.

Therefore:
1. **Full path** (e.g. `W8 C:\Users\<you>\Desktop\out.com`) → written **verbatim** to that location. Desktop/Documents/etc. are NOT virtualized, so the file should appear exactly there. **Recommended approach.**
2. **Bare name** (e.g. `W8 getkey2.com`) → goes to `%LOCALAPPDATA%\z80cpmw\data`, which on the Store build is **redirected** by MSIX file-system virtualization to:
   `…\Packages\AaronWohl.Z80CPM_<hash>\LocalCache\Local\z80cpmw\data\`
   (This redirect is a relocation, not an access denial. It is the original "where did my file go?" confusion — now surfaced correctly in About / Settings / banner.)

### Test plan (on the installed Store build)
- [ ] `W8 C:\Users\<you>\Desktop\test.txt` → confirm the file appears on the Desktop (validates full-path write works under MSIX full trust).
- [ ] `W8 getkey2.com` (bare) → find it; confirm it is under `…\Packages\AaronWohl.Z80CPM_*\LocalCache\Local\z80cpmw\data\`.
- [ ] Cross-check that path against what About / Settings → Open Folder / boot banner display (they should all agree).
- [ ] `R8 C:\Users\<you>\Desktop\getkey2.com` → confirm read of an arbitrary user-path file works.

### Follow-up (pending user decision)
- Offered but NOT yet done: add a one-line clarification to the Getting Started / Configuration help —
  *"Full paths to your own files (Desktop, Documents…) work even on the Store version; only bare names go to the app's redirected data folder."*
  Decide after the test confirms behavior.

## Other pending / not done
- Not pushed past `b4c36d2` (nothing newer to push except this WIP doc, which is committed but per request **not pushed**).
- MSIX 1.0.15 package not rebuilt.
- GUI behaviors not auto-verified (keystroke delivery to CP/M, mouse copy/paste rendering, first-run Help auto-open visuals) — need a hands-on pass.
