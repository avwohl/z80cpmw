# Work In Progress — z80cpmw

Working notes / handoff. Not user-facing docs, and not a record of what shipped:
**what shipped is in [`CHANGELOG.md`](CHANGELOG.md)**. Open items only.

The 1.0.15 round this file used to describe is finished and released, and its
history was moved out — the keymap, mouse copy/paste and About path fix are
`[1.0.15]`, the window position/size, auto-size and scrollable Help are
`[1.0.16-beta]`, the R8/W8 absolute-path handling and data-folder display are
`[1.0.14]`, and the MSIX packaging story from 1.0.20 through the Store release
is `[1.0.20]`, `[1.0.21-beta]` and `[1.0.22]`. The W8-under-MSIX question that
was this file's open question is answered, in source and in
[`docs/FILE_TRANSFER.md`](docs/FILE_TRANSFER.md).

Current version: **1.0.22** (Store, 2026-08-23), with the same build signed for
sideloading as 1.0.22-beta. Since `31d01c6` the version is edited only in
`z80cpmw/Version.h`; the MSIX and NSIS scripts derive theirs from it.

## This checkout cannot be rebuilt as it stands

`z80cpmw/z80cpmw.vcxproj` uses `PlatformToolset` `v145` (Visual Studio 18) and
expects the wxWidgets headers under `C:\temp\vcpkg\installed\x64-windows\include`.
That vcpkg tree is not present on this machine. `bin\Debug\z80cpmw.exe` is a
stale 1.0.19.0 build.

## Not verified on hardware

The behaviour is confirmed in source and documented; what has never happened is
a run against the **installed Store build**, which is the only thing that can
reproduce MSIX file-system redirection — a local unpackaged build cannot.

- [ ] `W8 C:\Users\<you>\Desktop\test.txt` → the file appears on the Desktop
      (full-path write works under MSIX full trust).
- [ ] `W8 getkey2.com` (bare name) → lands under
      `…\Packages\AaronWohl.Z80CPM_*\LocalCache\Local\z80cpmw\data\`.
- [ ] That path agrees with what About, Settings → Open Folder, and the boot
      banner display.
- [ ] `R8 C:\Users\<you>\Desktop\getkey2.com` → reading an arbitrary user path
      works.

Also unverified, and not automatable from here: keystroke delivery to CP/M,
mouse copy/paste rendering, and the first-run Help auto-open visuals. Each
needs a hands-on pass.

## Release chore

`dist/z80cpmw-1.0.22-beta.msix` — the signed sideload twin of the Store build —
has not been published as a GitHub release.
