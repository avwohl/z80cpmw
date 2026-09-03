# z80cpmw

Win32 + wxWidgets front end for a Z80 / CP/M (RomWBW HBIOS) emulator. Ships on
two channels: the Microsoft Store (unsigned MSIX, Microsoft re-signs) and a
signed sideload beta (Azure Trusted Signing, GitHub release).

## Build

Needs two sibling checkouts beside this one — `../cpmemu` and `../romwbw_emu` —
because `z80cpmw.vcxproj` compiles core sources straight out of them. They are
not submodules and nothing fetches them.

    MSBuild z80cpmw.sln -p:Configuration=Release -p:Platform=x64 -t:Rebuild -m
    cmd /c tests\run_tests.bat        # six headless suites, ~1325 checks

The suites need no wxWidgets, no vcpkg and no window (except the rendering one,
which opens its own). They run on any machine with a compiler. wxWidgets comes
from `C:\temp\vcpkg`.

`MainWindow.cpp` and `SettingsDialogWx.cpp` are in **no** suite and cannot be —
they need a real window and an interactive window station. Changes there are
verified by driving the built app with `WM_COMMAND` + `PrintWindow`. See
`WIP.md` for what a driving script must know (pointer-carrying common-control
messages are not marshalled across processes and will crash the app).

## Rules

**Disk images come from the ioscpm release area. Always.** Every port's client
downloads them from the catalog pinned by `RELEASE_TAG` in `DiskCatalog.cpp`.
Nothing is bundled in any package — if a packaging script grows a
`Copy-Item ...disks\*` or a `File ...hd1k_*.img`, that is a bug. When the
question is "which disk image do users get?", the answer is `RELEASE_TAG`,
never the build. Changing it is a code change with its own release.

**Never sign a `-Beta` package run on a version that is already published.**
`build-msix.ps1` names its output from `Version.h`, so such a run re-mints the
published artifact under its own name and nothing in the script objects. It has
happened once, on 2026-08-28, and cost a live signing call. Check
`gh release list` against `Version.h` first. `-WhatIf` is a hard binding error
there and is not a way to find out; the safe rehearsal is
`-Beta -SkipBuild -SkipSign`, which writes a distinct `-unsigned` name and
reaches neither `sign.ps1` nor the network.

**Both channels share a version number only when they carry the same binary.**
Cut the beta with `-SkipBuild` off the same `bin\Release` the Store package was
made from. A rebuild is a different binary; if the builds differ, the numbers
must differ too.

**A `.pdb` cannot be recovered after the fact.** A rebuild has a different debug
GUID, so its symbols will not load against a shipped binary. Both packaging
scripts keep the `.pdb` beside their output and fail if it is missing. 1.0.22
shipped without one and its crash dumps are permanently unreadable.

**Say what was measured, not what was inferred.** This tree has repeatedly
carried claims that were reasoned from one side of a mechanism without checking
the other — a caveat about images the packaging scripts *copy*, without checking
whether anything *reads* them; parity cells filled in from commit messages
without opening the port's source. If an entry cannot name the command that was
run or the symbol that was grepped, it is a guess and must say so.

**Cross-port claims need a citation that resolves.** `FEATURE_PARITY.md` marks
prose about a sibling with `<!-- cites: repo -->`, and every backticked
identifier inside must resolve by `git grep` in that port at the recorded
commit. `tools/check-sibling-drift.sh` enforces it and also reports how far each
port's tree has moved past what its column was read at. Run it before trusting
that document.

## todo.txt

Open items for **this** project only: bugs to fix and features to build, for the
next release. It is not a place for standing rules (they belong here), for
finished work (that belongs in `CHANGELOG.md`), or for notes about what some
other repository ought to do — `cpmemu`, `romwbw_emu`, `cpmdroid` and `ioscpm`
each have their own. **A closed item is deleted, not annotated.** If the file is
bigger at the end of a session than at the start, that session did not do its
job.

Related files: `MANUAL_CHECKS.md` (checks needing a person at a keyboard),
`KNOWN_PROBLEMS.md` (standing facts that will never be "done"), `WIP.md`
(longer-form handoff notes).
