# z80cpmw

Win32 + wxWidgets front end for a Z80 / CP/M (RomWBW HBIOS) emulator. Ships on
two channels: the Microsoft Store (unsigned MSIX, Microsoft re-signs) and a
signed sideload beta (Azure Trusted Signing, GitHub release).

## Build

Needs two sibling checkouts beside this one — `../cpmemu` and `../romwbw_emu` —
because `z80cpmw.vcxproj` compiles core sources straight out of them. They are
not submodules and nothing fetches them.

    MSBuild z80cpmw.sln -p:Configuration=Release -p:Platform=x64 -t:Rebuild -m
    cmd /c tests\run_tests.bat        # eight headless suites

The suites need no wxWidgets, no vcpkg and no window (except the rendering one,
which opens its own). They run on any machine with a compiler. wxWidgets comes
from `C:\temp\vcpkg`.

`MainWindow.cpp` and `SettingsDialogWx.cpp` are in **no** suite and cannot be —
they need a real window and an interactive window station. Changes there are
verified by driving the built app with `WM_COMMAND` + `PrintWindow`. See
`WIP.md` for what a driving script must know (pointer-carrying common-control
messages are not marshalled across processes and will crash the app).

## Rules

**ROMs and disk images come from the `romwbw_disks` catalog. Always.** There is
no `RELEASE_TAG` in this tree any more and no pinned release of anything: the
only URL compiled in is the index (`INDEX_URL` in `CatalogV0.cpp`), and it names
**no tag at all** - it goes through `releases/latest/download/`, so where the
index lives belongs to romwbw_disks and can move with no release here. From it
the client reads the RomWBW **releases** on offer — all of them, no build of this
one decides which — and fetches the chosen release's own catalog for the ROM and
the images.

**One kind of entry is not a release and is hidden by default.** romwbw_disks
began publishing RomWBW development snapshots on 2026-09-18, flagged
`prerelease: true` in the index, and `CATALOG_SCHEMA.md` 2.3 requires that a
client "MUST NOT offer a prerelease entry by default". `core.showPrereleaseVersions`
(the "Show development snapshots" box under the release picker) is that opt-in,
off by default. `catalogv0::isOffered` is the only place the rule lives, so the
picker and the automatic choice cannot disagree; it keeps the release a machine
is already on visible whatever the box says, because moving a machine off the
release its mounted images were built for is the mismatch the picker exists to
prevent. **It is the opposite kind of filter to the deleted one** — that one
asked a compile-time list what this binary could run, and so went stale in a
shipped build; this asks the document what upstream has published, and cannot. **So publishing a ROM, a disk image, or a whole RomWBW release is the
whole of shipping it — no build of this application is involved.** Verified on
2026-09-07 by running it: the release dropdown came back holding both published
releases with 3.6.0 selected, and the disk list held the 3.6.0 set including
`hd1k_infocom`, an id that has never existed in any build of this client.

**A whole new RomWBW release used to be the exception, and stopped being one on
2026-09-17.** What stood here was: `emu_romwbw_release_supported()` answered from
`ROMWBW_SUPPORTED_RELEASES` in `romwbw_emu/src/romwbw_pin.h`, a compile-time list
of two, so a 3.7.0 index entry was filtered out by any binary built before
somebody added it there — and publishing 3.7.0 cost a release of this application
and of the other four ports. romwbw_emu v1.44 deleted the function, the list and
the header; `catalogv0::runnableVersions` and its caller went with them here on
2026-09-17, unpackaged as this is written. The reasoning, in full in
`romwbw_emu/DOWNSTREAM.md` under "RomWBW releases are not this core's
business": the release number is the
HBIOS-to-CBIOS pairing of a ROM with a disk image, enforced by the guest's own
`*** WARNING: HBIOS/CBIOS Version Mismatch ***`, and not a property of the
emulator. What the emulator depends on is the emulator-to-ROM interface — two
I/O ports and the HBIOS functions `hbios_dispatch.cc` services — and that is
versioned by the name of the document `INDEX_URL` names: v0. A change the core
could not service would be published as `index-v1.json`, which this build ignores
by name. `emu_validate_rom_hcb` is still called on every load and still refuses a
ROM with no readable HCB, but it no longer judges a release, so it is not a
second gate behind the deleted one.

**What still has to match is the ROM and the disks, and nothing in this tree
enforces it.** The guest does. Ship a 3.6.0 ROM with 3.6.0 images; the Settings
dialog's note under the release picker says so when they disagree, and Start
offers to fetch the release's own ROM.

**Nothing is bundled in any package. There is no exception and no fallback
ROM.** If a packaging script grows a `Copy-Item ...disks\*`, a `File
...hd1k_*.img` or anything staging a `.rom`, that is a bug.

This paragraph used to say the opposite — that `roms\emu_avw.rom` and
`roms\emu_romwbw.rom` were tracked here and staged by a `PostBuildEvent`, "so a
first launch with no network still boots". Commit `6496fd4` deleted the `roms\`
directory and the staging; the `PostBuildEvent` at `z80cpmw.vcxproj:165-181`
copies the app-local VC++ runtime and says `NO ROM STAGING` in as many words.
A first launch with no network does **not** boot, and that is the design: every
ROM comes from the catalog and is checked against the size and sha256 only the
catalog carries, and this build will not load a ROM it cannot check.

When the question is "which disk image do users get?", the answer is whatever
`romwbw_disks` publishes at the release the user has selected, never the build.

**The catalog index itself is changeable, so "the catalog" is not always the
published one.** `Config::catalogIndexUrl` (empty = the index this build ships
with) and `$ROMWBW_INDEX_URL` (wins over it, for one run) point the client at
another `romwbw_disks` catalog — to test a release before publishing it, or to
run a fork. Precedence is copied from `romwbw_emu/tools/romwbw-get`, so one set
of instructions covers every client. `catalogv0::indexUrl()` is the only place
that resolves it and `catalogv0::indexUrlFromEnvironment()` the only reader of
the variable.

Two consequences worth knowing before answering a bug report:

- **Every catalog shares one data folder.** `catalogv0::indexScope()` computes
  the per-index suffix and **has no caller** — the isolation is not built. Two
  catalogs publishing an image under one name share one file. Downloads are safe
  (fetched beside the real name, renamed on only after the sha256 passes) and
  Start says so when a mounted image is not the one the catalog names, but the
  file is still shared. `todo.txt` carries the work.
- **A custom index is not a network fault.** A catalog that does not publish the
  selected release cannot supply its ROM however good the connection is, and the
  "Cannot start" dialog now names the index rather than blaming the network.

**Neither package can go down the other channel.** The `-Beta` package's
`Publisher` is rewritten to `CN=Aaron Wohl`, and Partner Center rejects it on
identity; the Store package is unsigned, so it will not sideload. Each goes to
its own channel and nowhere else.

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
scripts keep the `.pdb` beside their output and fail if it is missing — but
`build-msix.ps1` did that on its `-Beta` arm only until 1.0.25, so the rule was
true of this file before it was true of the code. 1.0.22 and 1.0.24 both shipped
Store packages with no symbols kept anywhere, and their crash dumps are
permanently unreadable. Both artifacts carry the version in the
name now (`z80cpmw-<ver>-store.{msix,pdb}`, since 2026-09-10), so neither a
package nor its symbols can be overwritten by the next Store build.

**Say what was measured, not what was inferred.** This tree has repeatedly
carried claims that were reasoned from one side of a mechanism without checking
the other — a caveat about images the packaging scripts *copy*, without checking
whether anything *reads* them; parity cells filled in from commit messages
without opening the port's source. If an entry cannot name the command that was
run or the symbol that was grepped, it is a guess and must say so.

**Cross-port claims need a citation that resolves.** `FEATURE_PARITY.md` marks
prose about a sibling with `<!-- cites: repo -->`, and every backticked
identifier inside must resolve by `git grep` in that port at the recorded
commit, and the `sibling-readings` block records which commit that is.

**Nothing checks this any more, and there is no longer a tool that could.**
`tools/check-sibling-drift.sh` resolved every one of those identifiers and
confirmed every recorded sha; it was deleted on 2026-09-13, after the Parity
gate workflow that ran it, the store-version jobs, and the `shipped:` field.
This repository has **no CI at all** and no parity tooling. That costs no test
coverage — no workflow here ever built or tested anything, `MSBuild` and
`run_tests.bat` needing Windows while the runners are Linux — but it does mean
the rule above is now enforced by nothing but whoever is writing.

So: **a citation is a promise you are making by hand.** The convention exists
because nine symbols were once cited for `cpmdroid` that existed nowhere in it,
four of the claims resting on them asserting the opposite of what that code
does. Before adding or editing a `cites:` region, `git grep` each identifier in
that port at the sha the block records, and say in the prose that you did.

## Searching this tree without a permission prompt

Use `Grep`, `Glob` and `Read`, not `grep`/`find`/`cat` through Bash. The
dedicated tools do not prompt; a Bash pipeline prompts once **per segment**, so
one `cat x && grep y | head` is three interruptions. This is also a rule for
subagents and workflows — a fan-out of seven investigators each running
`git show` is where the prompt floods have come from, so tell them the same
thing when you write their prompts, and prefer running the handful of `git` /
`gh` commands yourself in one place.

The prompts themselves are decided by `.claude/settings.json`, never by this
file: `permissions.allow` there lists the read-only Bash forms (`grep:*`,
`git log:*`, `gh release list:*`, …) plus `Read`/`Glob`/`Grep`. Add to that
list when a new read-only command starts prompting. Note that Claude Code only
watches for settings changes in directories that already had a settings file
when the session started, so the first session that *creates* `.claude/` keeps
prompting until `/config` is opened once or the session restarts.

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

## What is finished but not shipped

`tools/unreleased.sh` reports the gap between written, built, submitted and
served. It measures the Microsoft Store with `check-store-version.sh`, anchors
on the commit that set `VERSION_PATCH` to the served value (the Store channel
leaves no tag behind, so that is the best anchor there is), and lists what has
landed since — counting the ones touching `z80cpmw/` separately, since those are
the ones a Store user does not have. It also reads the sideload channel, which
*does* leave GitHub releases behind, but cannot see `dist\` on the Windows
machine and says so.

**It is not a gate and must not become one.** No exit 1: 0 even when the answer
is "fourteen commits unreleased", 2 only when it could not measure. Four jobs in
this family went red daily for that normal state and all four were deleted on
2026-09-13.

